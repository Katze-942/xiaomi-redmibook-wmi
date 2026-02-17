// SPDX-License-Identifier: GPL-2.0
/*
 * WMI driver for Xiaomi Redmi Book Pro 16 2025
 *
 * EVENT GUID 46C93E13-... (notify_id 0x20) — Fn hotkeys & mic-mute LED
 *   The BIOS stores the actual key function code in EVFN and builds an EVBU
 *   buffer via the _WED/EV20 method.  The buffer layout is:
 *     EVBU[0] = event type  (1 = key event)
 *     EVBU[1] = function id (0x21 = mic-mute, 0x07 = Fn-lock, ...)
 *     EVBU[2] = extra data
 *
 *   Handled keys:
 *    - 0x21 (mic-mute): KEY_MICMUTE + platform::micmute LED (audio-micmute)
 *    - 0x1B (settings):  KEY_CONFIG
 *
 * Battery charge limit — charge_control_end_threshold on BAT0
 *   The EC OperationRegion ERAM (base 0xFE0B0300, 256 bytes) contains:
 *    - LONL (offset 0xA4): bit 0 enables charge limiting
 *    - HBDA (offset 0xA7): charge threshold as a percentage (e.g. 0x50 = 80%)
 *   The METHOD GUID B60BFB48-... is used only to detect firmware support;
 *   actual register access is done via direct MMIO for arbitrary percentages.
 */

#include <acpi/battery.h>
#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/wmi.h>

#include <uapi/linux/input-event-codes.h>

#define REDMIBOOK_WMI_EVENT_GUID  "46C93E13-EE9B-4262-8488-563BCA757FEF"
#define REDMIBOOK_WMI_METHOD_GUID "B60BFB48-3E5B-49E4-A0E9-8CFFE1B3434B"

/* EC OperationRegion ERAM — base address and size */
#define EC_PHYS_BASE	0xFE0B0300
#define EC_MAP_SIZE	0x100

/* Mic-mute LED */
#define EC_LED_OFFSET	0x17
#define EC_LED_BIT	4		/* bit 4 in byte at offset 0x17 */

/* Battery charge limit */
#define EC_LONL_OFFSET	0xA4		/* bit 0: charge-limit enable */
#define EC_HBDA_OFFSET	0xA7		/* charge threshold (percent) */

/* EVBU[0]: event type */
#define EVBU_TYPE_KEY	1

/* EVBU[1]: function identifiers (from DSDT _Q handlers -> QV20 calls) */
#define EVBU_FN_MICMUTE		0x21
#define EVBU_FN_SETTINGS	0x1B

struct xiaomi_redmibook_wmi {
	struct input_dev *input_dev;
	struct led_classdev led_cdev;
	struct mutex key_lock;		/* serialises key event pairs */
	struct mutex ec_lock;		/* serialises EC register access */
	void __iomem *ec_base;
};

/*
 * Battery charge limit globals.
 * The ACPI battery hook callbacks receive only a struct power_supply pointer,
 * so there is no way to recover per-device driver data from them.
 */
static void __iomem *xiaomi_redmibook_ec_base;
static DEFINE_MUTEX(charge_lock);

/* Must be called with charge_lock held */
static int xiaomi_redmibook_ec_set_charge_threshold(int percent)
{
	u8 lonl;

	if (!xiaomi_redmibook_ec_base)
		return -ENODEV;

	lonl = ioread8(xiaomi_redmibook_ec_base + EC_LONL_OFFSET);
	if (percent < 100) {
		lonl |= BIT(0);
		iowrite8(lonl, xiaomi_redmibook_ec_base + EC_LONL_OFFSET);
		iowrite8(percent, xiaomi_redmibook_ec_base + EC_HBDA_OFFSET);
	} else {
		lonl &= ~BIT(0);
		iowrite8(lonl, xiaomi_redmibook_ec_base + EC_LONL_OFFSET);
		iowrite8(100, xiaomi_redmibook_ec_base + EC_HBDA_OFFSET);
	}

	return 0;
}

static int xiaomi_redmibook_ec_get_charge_threshold(void)
{
	u8 lonl, hbda;

	if (!xiaomi_redmibook_ec_base)
		return -ENODEV;

	lonl = ioread8(xiaomi_redmibook_ec_base + EC_LONL_OFFSET);
	if (!(lonl & BIT(0)))
		return 100;

	hbda = ioread8(xiaomi_redmibook_ec_base + EC_HBDA_OFFSET);
	return hbda;
}

static ssize_t charge_control_end_threshold_show(struct device *dev,
						 struct device_attribute *attr,
						 char *buf)
{
	int val;

	mutex_lock(&charge_lock);
	val = xiaomi_redmibook_ec_get_charge_threshold();
	mutex_unlock(&charge_lock);

	if (val < 0)
		return val;

	return sysfs_emit(buf, "%d\n", val);
}

static ssize_t charge_control_end_threshold_store(struct device *dev,
						  struct device_attribute *attr,
						  const char *buf, size_t count)
{
	int val, ret;

	ret = kstrtoint(buf, 10, &val);
	if (ret)
		return ret;

	if (val < 1 || val > 100)
		return -EINVAL;

	mutex_lock(&charge_lock);
	ret = xiaomi_redmibook_ec_set_charge_threshold(val);
	mutex_unlock(&charge_lock);

	return ret ? ret : count;
}

static DEVICE_ATTR_RW(charge_control_end_threshold);

static int xiaomi_redmibook_battery_add(struct power_supply *battery,
				 struct acpi_battery_hook *hook)
{
	return device_create_file(&battery->dev,
				  &dev_attr_charge_control_end_threshold);
}

static int xiaomi_redmibook_battery_remove(struct power_supply *battery,
				    struct acpi_battery_hook *hook)
{
	device_remove_file(&battery->dev,
			   &dev_attr_charge_control_end_threshold);
	return 0;
}

static struct acpi_battery_hook xiaomi_redmibook_battery_hook = {
	.name = "Xiaomi Redmi Book Charge Control",
	.add_battery = xiaomi_redmibook_battery_add,
	.remove_battery = xiaomi_redmibook_battery_remove,
};

static void ec_led_set(struct xiaomi_redmibook_wmi *data, bool on)
{
	u8 val;

	mutex_lock(&data->ec_lock);
	val = ioread8(data->ec_base + EC_LED_OFFSET);
	if (on)
		val |= BIT(EC_LED_BIT);
	else
		val &= ~BIT(EC_LED_BIT);
	iowrite8(val, data->ec_base + EC_LED_OFFSET);
	mutex_unlock(&data->ec_lock);
}

static bool ec_led_get(struct xiaomi_redmibook_wmi *data)
{
	u8 val;

	mutex_lock(&data->ec_lock);
	val = ioread8(data->ec_base + EC_LED_OFFSET);
	mutex_unlock(&data->ec_lock);

	return !!(val & BIT(EC_LED_BIT));
}

static int xiaomi_redmibook_led_set(struct led_classdev *cdev,
			     enum led_brightness brightness)
{
	struct xiaomi_redmibook_wmi *data =
		container_of(cdev, struct xiaomi_redmibook_wmi, led_cdev);

	ec_led_set(data, brightness != LED_OFF);
	return 0;
}

static enum led_brightness xiaomi_redmibook_led_get(struct led_classdev *cdev)
{
	struct xiaomi_redmibook_wmi *data =
		container_of(cdev, struct xiaomi_redmibook_wmi, led_cdev);

	return ec_led_get(data) ? LED_ON : LED_OFF;
}

static int xiaomi_redmibook_wmi_probe(struct wmi_device *wdev, const void *context)
{
	struct xiaomi_redmibook_wmi *data;
	int ret;

	data = devm_kzalloc(&wdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	dev_set_drvdata(&wdev->dev, data);

	ret = devm_mutex_init(&wdev->dev, &data->key_lock);
	if (ret)
		return ret;

	ret = devm_mutex_init(&wdev->dev, &data->ec_lock);
	if (ret)
		return ret;

	data->ec_base = devm_ioremap(&wdev->dev, EC_PHYS_BASE, EC_MAP_SIZE);
	if (!data->ec_base) {
		dev_err(&wdev->dev, "failed to map EC memory at %#x\n",
			EC_PHYS_BASE);
		return -ENOMEM;
	}

	data->input_dev = devm_input_allocate_device(&wdev->dev);
	if (!data->input_dev)
		return -ENOMEM;

	data->input_dev->name = "Xiaomi Redmi Book WMI keys";
	data->input_dev->phys = "xiaomi-redmibook-wmi/input0";
	data->input_dev->id.bustype = BUS_HOST;

	set_bit(EV_KEY, data->input_dev->evbit);
	set_bit(KEY_MICMUTE, data->input_dev->keybit);
	set_bit(KEY_CONFIG, data->input_dev->keybit);

	ret = input_register_device(data->input_dev);
	if (ret) {
		dev_err(&wdev->dev, "failed to register input device\n");
		return ret;
	}

	data->led_cdev.name = "platform::micmute";
	data->led_cdev.max_brightness = LED_ON;
	data->led_cdev.brightness_set_blocking = xiaomi_redmibook_led_set;
	data->led_cdev.brightness_get = xiaomi_redmibook_led_get;
	data->led_cdev.default_trigger = "audio-micmute";

	ret = devm_led_classdev_register(&wdev->dev, &data->led_cdev);
	if (ret) {
		dev_err(&wdev->dev, "failed to register LED device\n");
		return ret;
	}

	/* Battery charge limit — only if firmware advertises METHOD GUID */
	if (wmi_has_guid(REDMIBOOK_WMI_METHOD_GUID)) {
		xiaomi_redmibook_ec_base = data->ec_base;

		ret = devm_battery_hook_register(&wdev->dev,
						 &xiaomi_redmibook_battery_hook);
		if (ret) {
			dev_warn(&wdev->dev,
				 "failed to register battery hook: %d\n", ret);
			xiaomi_redmibook_ec_base = NULL;
			/* Non-fatal: key/LED functionality still works */
		}
	}

	dev_info(&wdev->dev, "Xiaomi Redmi Book WMI driver loaded\n");
	return 0;
}

static void xiaomi_redmibook_wmi_notify(struct wmi_device *wdev,
				 union acpi_object *obj)
{
	struct xiaomi_redmibook_wmi *data = dev_get_drvdata(&wdev->dev);
	unsigned int key;
	u8 *buf;

	if (!obj || obj->type != ACPI_TYPE_BUFFER || obj->buffer.length < 2) {
		dev_dbg(&wdev->dev, "unexpected WMI event data\n");
		return;
	}

	buf = obj->buffer.pointer;

	if (buf[0] != EVBU_TYPE_KEY) {
		dev_dbg(&wdev->dev, "ignoring WMI event type=%u\n", buf[0]);
		return;
	}

	switch (buf[1]) {
	case EVBU_FN_MICMUTE:
		key = KEY_MICMUTE;
		break;
	case EVBU_FN_SETTINGS:
		key = KEY_CONFIG;
		break;
	default:
		dev_dbg(&wdev->dev, "unhandled WMI fn=%#x\n", buf[1]);
		return;
	}

	mutex_lock(&data->key_lock);
	input_report_key(data->input_dev, key, 1);
	input_sync(data->input_dev);
	input_report_key(data->input_dev, key, 0);
	input_sync(data->input_dev);
	mutex_unlock(&data->key_lock);
}

static const struct wmi_device_id xiaomi_redmibook_wmi_id_table[] = {
	{ REDMIBOOK_WMI_EVENT_GUID, NULL },
	{ }
};
MODULE_DEVICE_TABLE(wmi, xiaomi_redmibook_wmi_id_table);

static struct wmi_driver xiaomi_redmibook_wmi_driver = {
	.driver = {
		.name = "xiaomi-redmibook-wmi",
	},
	.id_table	= xiaomi_redmibook_wmi_id_table,
	.probe		= xiaomi_redmibook_wmi_probe,
	.notify		= xiaomi_redmibook_wmi_notify,
	.no_singleton	= true,
};
module_wmi_driver(xiaomi_redmibook_wmi_driver);

MODULE_AUTHOR("katze_942");
MODULE_DESCRIPTION("WMI driver for Xiaomi Redmi Book Pro 16 2025");
MODULE_LICENSE("GPL");
