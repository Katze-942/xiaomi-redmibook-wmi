[Русская версия ниже / Russian version below](#xiaomi-redmibook-wmi-1)

# xiaomi-redmibook-wmi

WMI kernel driver for Xiaomi REDMI Book Pro 16 2025.

- Fn+MicMute — microphone mute/unmute with LED indicator
- Fn+Settings — opens system settings
- Battery charge limit (any threshold from 1 to 100%)

P.S: I haven't actually tested 1% though :D

## Installation

Requires DKMS and kernel headers for your running kernel.

```bash
run0 ./install-dkms.sh
```

The module is loaded automatically after installation, no reboot required.

## Removal

```bash
run0 ./uninstall-dkms.sh
```

## Battery charge limit

```bash
# Read current threshold
cat /sys/class/power_supply/BAT0/charge_control_end_threshold

# Set to 80%
echo 80 | run0 tee /sys/class/power_supply/BAT0/charge_control_end_threshold
```

KDE Plasma and GNOME automatically detect `charge_control_end_threshold` and expose it in their power settings UI.

Tested on:
- Xiaomi Redmi Book Pro 16 2025 (TM2409)
- ALT Regular KDE (kernel 6.19)

## FAQ

**Q: I switch to a different microphone in the DE and mute it, but the LED doesn't turn on. Why?**

A: The LED is driven by the `audio-micmute` ALSA trigger (`snd_ctl_led`), which monitors specific ALSA `Capture Switch` controls. When you switch the default source in PipeWire to a different device, `snd_ctl_led` may still be tracking the `Capture Switch` of the previous device, so it doesn't see the mute state change.

---

# xiaomi-redmibook-wmi

WMI kernel-драйвер для Xiaomi REDMI Book Pro 16 2025.

- Fn+MicMute — включение/выключение микрофона с LED-индикатором на клавише
- Fn+Settings — открывает системные настройки
- Ограничение заряда батареи (произвольный порог от 1 до 100%)

P.S: ладно я ещё не тестировал 1% :D

## Установка

Требуется DKMS и заголовки ядра для текущего ядра.

```bash
run0 ./install-dkms.sh
```

Модуль загружается автоматически после установки, перезагрузка не требуется.

## Удаление

```bash
run0 ./uninstall-dkms.sh
```

## Ограничение заряда батареи

```bash
# Прочитать текущий порог
cat /sys/class/power_supply/BAT0/charge_control_end_threshold

# Установить 80%
echo 80 | run0 tee /sys/class/power_supply/BAT0/charge_control_end_threshold
```

KDE Plasma и GNOME автоматически обнаруживают `charge_control_end_threshold` и отображают его в настройках питания.

## Совместимость

Протестировано на:
- Xiaomi Redmi Book Pro 16 2025 (TM2409)
- ALT Regular KDE (ядро 6.19)

## FAQ

**Вопрос: я переключаюсь на другой микрофон в DE и мьючу его, но LED не загорается. Почему?**

Ответ: LED управляется ALSA-триггером `audio-micmute` (модуль `snd_ctl_led`), который отслеживает конкретные ALSA-контролы `Capture Switch`. При переключении default source в PipeWire на другое устройство `snd_ctl_led` может продолжать следить за `Capture Switch` предыдущего устройства и не видит изменение состояния мьюта.
