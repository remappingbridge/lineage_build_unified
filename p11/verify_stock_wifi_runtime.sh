#!/bin/bash
set -euo pipefail

ADB="${ADB:-adb}"

echo "Checking Lenovo Tab P11 stock-kernel Wi-Fi baseline..."
"$ADB" root >/dev/null 2>&1 || true
"$ADB" wait-for-device

KREL="$("$ADB" shell uname -r | tr -d '\r')"
PROC_VERSION="$("$ADB" shell cat /proc/version | tr -d '\r')"

echo "Kernel: $KREL"
if [ "$KREL" != "4.19.157-perf+" ]; then
    echo "FAIL: expected Lenovo stock kernel release 4.19.157-perf+." >&2
    echo "Do not validate Wi-Fi with a rebuilt/custom boot image." >&2
    exit 1
fi

if [[ "$PROC_VERSION" != *"clang version 10.0.7"* ]]; then
    echo "WARN: kernel release matches, but compiler signature differs from the captured Lenovo stock boot."
fi

if ! "$ADB" shell ip link show wlan0 >/dev/null 2>&1; then
    echo "FAIL: wlan0 is absent." >&2
    "$ADB" shell 'ip link show; ps -A | grep -Ei "wificond|wpa_supplicant|wifi|wlan" || true'
    exit 1
fi

echo
"$ADB" shell cmd wifi status || true
echo

"$ADB" shell cmd wifi start-scan >/dev/null 2>&1 || true
sleep 5
SCAN="$("$ADB" shell cmd wifi list-scan-results 2>&1 | tr -d '\r')"
printf '%s\n' "$SCAN"

if ! printf '%s\n' "$SCAN" | grep -Eqi '([0-9a-f]{2}:){5}[0-9a-f]{2}'; then
    echo "FAIL: Wi-Fi scan returned no BSSIDs." >&2
    echo "Collecting framework/process diagnostics:" >&2
    "$ADB" shell 'cmd wifi status; echo; ps -A | grep -Ei "wificond|wpa_supplicant|wifi|wlan"; echo; service list | grep -Ei "wifi|wificond|supplicant"' || true
    exit 1
fi

echo
echo "OK: wlan0 exists and Wi-Fi scanning returns nearby BSSIDs."
echo "Trackpad bridge state:"
"$ADB" shell getprop persist.bluetooth.p11_mt2_bridge || true
