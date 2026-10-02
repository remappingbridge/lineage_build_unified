# LineageOS 22 Light — Lenovo Tab P11 stock-kernel Wi-Fi + Magic Trackpad profile

This branch preserves the working Magic Trackpad gesture stack while removing the
kernel rebuild from the P11 runtime path.

## Why

Physical testing established two separate facts:

- the rebuilt P11 `4.19.157-perf+` kernel can expose Magic Trackpad 2 multitouch,
  but Wi-Fi can be enabled without discovering nearby networks;
- the known-good Andy Yan LineageOS 22.2 GSI scans/connects on the Lenovo stock
  kernel/vendor combination.

The Wi-Fi regression therefore must not be "fixed" in Trebuchet. The P11 profile
keeps the pinned Trebuchet gesture customization and applies the MT2
Bluetooth/UHID bridge in the Android Bluetooth stack, allowing the **stock Lenovo
boot/kernel** to remain in use for Qualcomm WLAN compatibility.

## Pinned Trebuchet

```text
repository: remappingbridge/trebuchet-lineage-22.2
commit: 96ddd23a6400962798c6d119ef165594425137f8
source branch: experimental/trackpad-4finger-allapps-home
```

## Build

Use this branch of `lineage_build_unified` and the existing
`lineage_patches_unified:lineage-22-light` checkout.

```bash
bash lineage_build_unified/build_unified.sh treble p11 64GN
```

The `p11` profile:

1. applies the MT2 Bluetooth/UHID bridge;
2. verifies all pinned P11 sources and that the bridge is integrated;
3. builds only the GSI system image;
4. names the result with `-p11-stock-kernel`.

It intentionally does **not** build, patch or flash a kernel.

For an existing synced source tree:

```bash
bash lineage_build_unified/build_unified.sh treble nosync p11 64GN
```

## Runtime rule

The P11 profile requires the Lenovo stock boot/kernel. Do not install the
recompiled Magic Trackpad kernel together with this GSI.

After installation, enable the bridge and reconnect the Magic Trackpad:

```bash
adb root
adb wait-for-device
adb shell setprop persist.bluetooth.p11_mt2_bridge true
```

Validate the stock-kernel Wi-Fi path with:

```bash
bash lineage_build_unified/p11/verify_stock_wifi_runtime.sh
```

The verifier requires `4.19.157-perf+`, checks `wlan0`, forces a scan, and
fails if no BSSIDs are returned.

Detailed rationale and migration notes are in
[P11_WIFI_TRACKPAD.md](P11_WIFI_TRACKPAD.md). The bridge implementation is
documented in [p11/mt2-bridge/README.md](p11/mt2-bridge/README.md).
