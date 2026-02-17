#!/bin/bash
set -euo pipefail

MODULE_NAME="xiaomi-redmibook-wmi"
MODULE_VERSION="1.0"
SRC_DIR="$(cd "$(dirname "$0")" && pwd)/xiaomi-redmibook-wmi"
DEST_DIR="/usr/src/${MODULE_NAME}-${MODULE_VERSION}"

if [ "$(id -u)" -ne 0 ]; then
    echo "This script must be run as root" >&2
    exit 1
fi

if ! command -v dkms &>/dev/null; then
    echo "DKMS is not installed." >&2
    exit 1
fi

if dkms status "${MODULE_NAME}/${MODULE_VERSION}" 2>/dev/null | grep -q "${MODULE_NAME}"; then
    echo "Removing old module version..."
    dkms remove "${MODULE_NAME}/${MODULE_VERSION}" --all 2>/dev/null || true
fi

echo "Copying sources to ${DEST_DIR}..."
rm -rf "${DEST_DIR}"
mkdir -p "${DEST_DIR}"
cp "${SRC_DIR}/xiaomi-redmibook-wmi.c" "${DEST_DIR}/"
cp "${SRC_DIR}/Makefile" "${DEST_DIR}/"
cp "${SRC_DIR}/dkms.conf" "${DEST_DIR}/"
cp "${SRC_DIR}/fix-autoconf.sh" "${DEST_DIR}/"

echo "DKMS add..."
dkms add "${MODULE_NAME}/${MODULE_VERSION}"

echo "DKMS build..."
dkms build "${MODULE_NAME}/${MODULE_VERSION}"

echo "DKMS install..."
dkms install "${MODULE_NAME}/${MODULE_VERSION}"

echo "Loading module..."
modprobe "${MODULE_NAME}" || insmod "$(modinfo -n "${MODULE_NAME}" 2>/dev/null || echo "${DEST_DIR}/${MODULE_NAME}.ko")"

echo "Configuring autoload..."
mkdir -p /etc/modules-load.d
echo "${MODULE_NAME}" > "/etc/modules-load.d/${MODULE_NAME}.conf"

echo ""
echo "=== Done ==="
echo "Module ${MODULE_NAME} installed and loaded."
echo ""
echo "Verify:"
echo "  dkms status                                    — DKMS status"
echo "  lsmod | grep xiaomi_redmibook_wmi              — module is loaded"
echo "  ls /sys/class/leds/platform::micmute/          — LED registered"
echo "  cat /sys/class/leds/platform::micmute/trigger  — audio-micmute trigger"
echo "  cat /sys/class/power_supply/BAT0/charge_control_end_threshold"
