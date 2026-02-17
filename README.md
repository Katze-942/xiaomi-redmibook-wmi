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

### Persisting across reboots

The charge limit is automatically restored on reboot — the driver reads the last value from EC memory on startup. No extra setup needed.

Tested on:
- Xiaomi Redmi Book Pro 16 2025 (TM2409)
- ALT Regular KDE (kernel 6.19)

## FAQ

**Q: I switch to a different microphone in the DE and mute it, but the LED doesn't turn on. Why?**

A: The LED is driven by the `audio-micmute` ALSA trigger (`snd_ctl_led`), which monitors specific ALSA `Capture Switch` controls. When you switch the default source in PipeWire to a different device, `snd_ctl_led` may still be tracking the `Capture Switch` of the previous device, so it doesn't see the mute state change.

See [Workaround: pipewire-led-sync](#workaround-pipewire-led-sync) below.

## Workaround: pipewire-led-sync

A userspace daemon that disables the kernel `audio-micmute` trigger and takes full control of the LED, tracking mute state via PipeWire (`pactl subscribe`).

This correctly handles:
- Mute/unmute of any default source (built-in mic, USB mic, etc.)
- Switching the default source in the DE
- PipeWire restarts
- Device hotplug

### Installation

Requires `pactl` (provided by `pipewire-pulse` or `pulseaudio-utils`).

```bash
run0 ./pipewire-led-sync/install.sh
systemctl --user daemon-reload
systemctl --user enable --now mic-led-sync.service
```

### Removal

```bash
systemctl --user disable --now mic-led-sync.service
run0 ./pipewire-led-sync/uninstall.sh
```

The `audio-micmute` kernel trigger is restored automatically when the service stops.

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

### Сохранение порога после перезагрузки

Ограничение заряда автоматически восстанавливается при перезагрузке — драйвер считывает последнее значение из памяти EC при запуске. Дополнительная настройка не требуется.

## Совместимость

Протестировано на:
- Xiaomi Redmi Book Pro 16 2025 (TM2409)
- ALT Regular KDE (ядро 6.19)

## FAQ

**Вопрос: я переключаюсь на другой микрофон в DE и мьючу его, но LED не загорается. Почему?**

Ответ: LED управляется ALSA-триггером `audio-micmute` (модуль `snd_ctl_led`), который отслеживает конкретные ALSA-контролы `Capture Switch`. При переключении default source в PipeWire на другое устройство `snd_ctl_led` может продолжать следить за `Capture Switch` предыдущего устройства и не видит изменение состояния мьюта.

См. [Обходной путь: pipewire-led-sync](#обходной-путь-pipewire-led-sync) ниже.

## Обходной путь: pipewire-led-sync

Userspace-демон, который отключает триггер ядра `audio-micmute` и берёт полный контроль над LED, отслеживая состояние мьюта через PipeWire (`pactl subscribe`).

Корректно обрабатывает:
- Мьют/анмьют любого default source (встроенный микрофон, USB-микрофон и т.д.)
- Переключение default source в DE
- Рестарты PipeWire
- Подключение/отключение устройств (hotplug)

### Установка

Требуется `pactl` (предоставляется пакетом `pipewire-pulse` или `pulseaudio-utils`).

```bash
run0 ./pipewire-led-sync/install.sh
systemctl --user daemon-reload
systemctl --user enable --now mic-led-sync.service
```

### Удаление

```bash
systemctl --user disable --now mic-led-sync.service
run0 ./pipewire-led-sync/uninstall.sh
```

Триггер ядра `audio-micmute` восстанавливается автоматически при остановке сервиса.
