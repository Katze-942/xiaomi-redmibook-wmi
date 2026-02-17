#!/bin/bash
# Workaround for ALT Linux: kernel-headers-modules package is missing
# include/generated/autoconf.h, which is required to build out-of-tree
# kernel modules. This script generates it from include/config/auto.conf.
# On distros where autoconf.h is already present, this script does nothing.

KDIR="${1:-/lib/modules/$(uname -r)/build}"
AUTOCONF="$KDIR/include/generated/autoconf.h"
AUTOCONF_SRC="$KDIR/include/config/auto.conf"

[ -f "$AUTOCONF" ] && exit 0
[ -f "$AUTOCONF_SRC" ] || exit 0

mkdir -p "$(dirname "$AUTOCONF")"

AUTOCONF_TMP="${AUTOCONF}.tmp.$$"

if awk '/^CONFIG_/{
    key = $0; sub(/=.*/, "", key)
    val = $0; sub(/^[^=]+=/, "", val)
    if (val == "") val = "\"\""
    if (val == "y") print "#define " key " 1"
    else if (val == "m") print "#define " key "_MODULE 1"
    else print "#define " key " " val
}' "$AUTOCONF_SRC" > "$AUTOCONF_TMP"; then
    mv "$AUTOCONF_TMP" "$AUTOCONF"
else
    rm -f "$AUTOCONF_TMP"
    echo "fix-autoconf.sh: failed to generate $AUTOCONF" >&2
    exit 1
fi
