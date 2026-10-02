# Lenovo Tab P11 — preserve Wi-Fi while keeping Magic Trackpad gestures

## Physical diagnosis

The trackpad and Wi-Fi regressions are independent:

- `lineage-22.2-20250621-UNOFFICIAL-gsi_arm64_gN-signed.img.gz` is the Wi-Fi
  reference: scanning and connection work on the Lenovo stock kernel/vendor path.
- `p11-baseline-4.19.157.img` already showed "Wi-Fi enabled, no networks found"
  before the Magic Trackpad kernel backport.
- `p11-magic-trackpad2-4.19.157.img` makes the Magic Trackpad multitouch path
  work, but retains the same Wi-Fi scan regression.

This makes the rebuilt kernel the regression boundary. The Trebuchet gesture
code is not part of WLAN initialization or scanning and is intentionally left
unchanged.

## Resolution architecture

Use these three pieces together:

```text
Lenovo stock boot / kernel 4.19.157-perf+
        -> preserves Qualcomm WLAN/vendor compatibility

LineageOS 22.2 system.img
        -> contains the P11 Bluetooth/UHID MT2 bridge

Pinned Trebuchet 96ddd23a...
        -> maps the resulting Android four-finger vertical gestures
           up -> All Apps
           down -> Home
```

The MT2 bridge converts Bluetooth Magic Trackpad 2 reports before UHID, so the
stock kernel's generic `hid-multitouch` path can expose Android touchpad input.
No custom kernel is required.

## Build profile

Branch:

```text
remappingbridge/lineage_build_unified
experimental/p11-stock-kernel-wifi-trackpad
```

Build from a normal synced workspace:

```bash
bash lineage_build_unified/build_unified.sh treble p11 64GN
```

For an already prepared/synced workspace:

```bash
bash lineage_build_unified/build_unified.sh treble nosync p11 64GN
```

The build stops if the P11 source pins or MT2 bridge integration do not match.

## Installation boundary

This profile produces a **system image only**. It must be paired with the Lenovo
stock boot image from the tablet/verified backup.

Do not flash the rebuilt `p11-magic-trackpad2-4.19.157.img` boot image when
testing this profile. Restoring/keeping stock boot is the Wi-Fi preservation
step.

Do not replace `vendor`, modem, persist, firmware or DTBO as part of this fix.

## Enable the bridge

After booting the rebuilt GSI on stock boot:

```bash
adb root
adb wait-for-device
adb shell setprop persist.bluetooth.p11_mt2_bridge true
```

Power-cycle/reconnect the Magic Trackpad so Bluetooth creates a new UHID device.

## Acceptance

Run:

```bash
bash lineage_build_unified/p11/verify_stock_wifi_runtime.sh
```

Accept the Wi-Fi side only if:

- kernel release is `4.19.157-perf+`;
- `wlan0` exists;
- scan results contain BSSIDs;
- the saved network can reconnect and pass traffic.

Then validate Trackpad:

- pointer and click;
- two-finger scrolling;
- three-finger navigation;
- four-finger up -> All Apps;
- four-finger down -> Home;
- reconnect and reboot.

If Wi-Fi still returns no BSSIDs while the stock Lenovo boot is confirmed, stop
and collect WLAN HAL/driver logs; do not change Trebuchet gesture code.
