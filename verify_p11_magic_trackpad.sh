#!/bin/bash
set -euo pipefail

check_repo() {
    local path="$1"
    local expected="$2"
    local label="$3"

    if [ ! -d "$path/.git" ]; then
        echo "FAIL: $label repository not found at $path" >&2
        return 1
    fi

    local actual
    actual="$(git -C "$path" rev-parse HEAD)"

    if [ "$actual" != "$expected" ]; then
        echo "FAIL: $label" >&2
        echo "  expected: $expected" >&2
        echo "  actual:   $actual" >&2
        return 1
    fi

    echo "OK: $label -> $actual"
}

echo "Verifying P11 Magic Trackpad source pins..."
echo

check_repo "packages/apps/Trebuchet"     "96ddd23a6400962798c6d119ef165594425137f8"     "Trebuchet Magic Trackpad"

check_repo "device/lineage/gsi"     "b529f626be220d441f1293dacb4c28112fbfae95"     "Andy Yan GSI device tree"

check_repo "packages/apps/QcRilAm"     "dc599b67cc1e7e9a62ca15eef605e3b2546d0d42"     "QcRilAm"

check_repo "vendor/hardware_overlay"     "2b84b6e5e6f04fbcf2d122bf985753c6b9c9bbca"     "TrebleDroid hardware overlays"

check_repo "lineage_patches_unified"     "20734839c3c228cde47f408d1583a10d067d1990"     "Andy Yan LineageOS 22 Light patches"

BT_CO="packages/modules/Bluetooth/system/btif/co/bta_hh_co.cc"
BRIDGE_HEADER="packages/modules/Bluetooth/system/btif/co/p11_mt2_bridge.h"

if [ ! -f "$BRIDGE_HEADER" ]; then
    echo "FAIL: P11 MT2 bridge header is not installed" >&2
    exit 1
fi

if ! grep -q 'p11_mt2_bridge.h' "$BT_CO"; then
    echo "FAIL: P11 MT2 bridge is not integrated into Bluetooth UHID writer" >&2
    exit 1
fi

python3 ./lineage_build_unified/p11/mt2-bridge/apply.py "$PWD" --check
echo "OK: P11 Bluetooth/UHID MT2 bridge"

echo
echo "All pinned P11 sources and the MT2 bridge match."
echo "Note: the LineageOS 22.2 platform manifest and MindTheGapps vic branch are not date-pinned."
