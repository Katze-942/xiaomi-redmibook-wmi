#!/bin/bash
set -euo pipefail

if [[ $EUID -ne 0 ]]; then
    echo "ERROR: run as root" >&2
    exit 1
fi

if ! command -v pactl &>/dev/null; then
    echo "ERROR: pactl not found, install pipewire-pulse (or pulseaudio-utils)" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "Installing mic-led-sync..."

install -Dm755 "$SCRIPT_DIR/mic-led-sync.sh" /usr/local/bin/mic-led-sync
echo "  Installed /usr/local/bin/mic-led-sync"

install -Dm644 "$SCRIPT_DIR/udev/99-xiaomi-micmute-led.rules" /etc/udev/rules.d/99-xiaomi-micmute-led.rules
echo "  Installed udev rule"

udevadm control --reload-rules || true
udevadm trigger --subsystem-match=leds --attr-match=name="platform::micmute" --action=add || true
echo "  Reloaded udev rules"

install -Dm644 "$SCRIPT_DIR/systemd/mic-led-sync.service" /usr/lib/systemd/user/mic-led-sync.service
echo "  Installed systemd user unit"

echo ""
echo "Done. Now run as your regular user:"
echo "  systemctl --user daemon-reload"
echo "  systemctl --user enable --now mic-led-sync.service"
