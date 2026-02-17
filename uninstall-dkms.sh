#!/bin/bash
set -euo pipefail

MODULE_NAME="xiaomi-redmibook-wmi"
MODULE_VERSION="1.0"

if [ "$(id -u)" -ne 0 ]; then
    echo "This script must be run as root" >&2
    exit 1
fi

echo "Unloading module..."
rmmod "${MODULE_NAME}" 2>/dev/null || true

if dkms status "${MODULE_NAME}/${MODULE_VERSION}" 2>/dev/null | grep -q "${MODULE_NAME}"; then
    echo "Removing from DKMS..."
    dkms remove "${MODULE_NAME}/${MODULE_VERSION}" --all
fi

echo "Removing sources..."
rm -rf "/usr/src/${MODULE_NAME}-${MODULE_VERSION}"

rm -f "/etc/modules-load.d/${MODULE_NAME}.conf"

echo ""
echo "=== Done ==="
echo "Module ${MODULE_NAME} removed."
