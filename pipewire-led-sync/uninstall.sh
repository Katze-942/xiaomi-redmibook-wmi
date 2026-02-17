#!/bin/bash
set -euo pipefail

if [[ $EUID -ne 0 ]]; then
    echo "ERROR: run as root" >&2
    exit 1
fi

echo "Uninstalling mic-led-sync..."
echo ""
echo "NOTE: First run as your regular user:"
echo "  systemctl --user disable --now mic-led-sync.service"
echo ""
read -rp "Continue with removal? [y/N] " answer
case "$answer" in
    [yY]*) ;;
    *) echo "Aborted."; exit 0 ;;
esac

rm -f /usr/local/bin/mic-led-sync
echo "  Removed /usr/local/bin/mic-led-sync"

rm -f /etc/udev/rules.d/99-xiaomi-micmute-led.rules
udevadm control --reload-rules || true
echo "  Removed udev rule"

rm -f /usr/lib/systemd/user/mic-led-sync.service
echo "  Removed systemd user unit"

TRIGGER="/sys/class/leds/platform::micmute/trigger"
if [[ -w "$TRIGGER" ]]; then
    echo audio-micmute > "$TRIGGER" && echo "  Restored LED trigger"
fi

echo ""
echo "Done."
