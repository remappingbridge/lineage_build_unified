# Lenovo Tab P11 — LineageOS 22 Light + Magic Trackpad experiment

## Goal

Build a LineageOS 22.2 Light GSI for the Lenovo Tab P11 using Andy Yan's Light build recipe while changing only the Trebuchet/Quickstep behavior needed for the Magic Trackpad four-finger vertical gestures.

The first physical build intentionally contains **no Wi-Fi fix**. Wi-Fi is a regression detector: the known-good Andy Yan GSI can scan and connect, so this experiment should preserve that behavior while testing the Trackpad customization independently.

## Known-good reference image

The physical reference previously used on the P11 is:

```text
lineage-22.2-20250621-UNOFFICIAL-gsi_arm64_gN-signed.img.gz
```

This repository branch is based on the Andy Yan `lineage-22-light` build-recipe snapshot from 2025-06-21:

```text
remappingbridge/lineage_build_unified
baseline branch: lineage-22-light
baseline commit: 18e56b676e9ee3dbf827dee1672fb8ecf6de5a60
experiment branch: experimental/p11-magic-trackpad
```

The Light patch set is frozen in:

```text
remappingbridge/lineage_patches_unified
branch: lineage-22-light
commit: 20734839c3c228cde47f408d1583a10d067d1990
```

## Only intentional functional change

The LineageOS Trebuchet project is removed from the local manifest and replaced with:

```text
repository: remappingbridge/trebuchet-lineage-22.2
source branch used to create the commit: experimental/trackpad-4finger-allapps-home
pinned commit: 0c24a7362d95fe3685714f8b8cb1ef25ae12fad3
path: packages/apps/Trebuchet
```

The Trackpad implementation originates in commit `0dc553b3eca0ff167eade41a7458014a493af86a`; the pinned commit above also adapts `SystemUiProxy` to the current LineageOS 22.2 `onKeyEvent(int)` interface.

The Trackpad implementation changes:

```text
quickstep/src/com/android/quickstep/TouchInteractionService.java
src/com/android/launcher3/Launcher.java
```

Expected behavior:

```text
four-finger swipe up   -> All Apps
four-finger swipe down -> Home
horizontal four-finger gestures -> existing Quickstep behavior
two/three-finger gestures       -> existing behavior
```

No kernel, Wi-Fi, Bluetooth stack, vendor or firmware code is intentionally changed by the Trackpad customization.

## Pinned GSI-specific sources

The experiment manifest pins the following sources:

```text
AndyCGYan/android_device_lineage_gsi
b529f626be220d441f1293dacb4c28112fbfae95

AndyCGYan/android_packages_apps_QcRilAm
dc599b67cc1e7e9a62ca15eef605e3b2546d0d42

TrebleDroid/vendor_hardware_overlay
2b84b6e5e6f04fbcf2d122bf985753c6b9c9bbca
```

The hardware-overlay commit is the latest commit on the `pie` branch at or before 2025-06-21.

## Reproducibility limitation

This is not yet a byte-for-byte reconstruction of the 2025-06-21 Andy Yan image.

The build recipe initializes:

```bash
repo init -u https://github.com/LineageOS/android.git -b lineage-22.2 --git-lfs
```

The LineageOS `lineage-22.2` branch continued moving after June 2025. Also, `MindTheGapps/vendor_gapps` is still selected through its moving `vic` branch, matching the structure of the original Andy Yan recipe.

Therefore this branch currently means:

```text
Andy Yan LineageOS 22 Light recipe
+ pinned GSI-specific components where known
+ pinned Magic Trackpad Trebuchet
+ current LineageOS 22.2 platform selected by repo sync
+ MindTheGapps vic
```

Do not describe the output as an exact rebuild of the 2025-06-21 binary.

## Build workspace

Use a clean workspace for the first experiment.

```bash
mkdir -p ~/lineage-22-build-gsi
cd ~/lineage-22-build-gsi

repo init -u https://github.com/LineageOS/android.git -b lineage-22.2 --git-lfs

git clone -b experimental/p11-magic-trackpad \
  https://github.com/remappingbridge/lineage_build_unified.git \
  lineage_build_unified

git clone -b lineage-22-light \
  https://github.com/remappingbridge/lineage_patches_unified.git \
  lineage_patches_unified
```

Because the custom Trebuchet repository is private, verify SSH access before running the sync:

```bash
git ls-remote git@github.com:remappingbridge/trebuchet-lineage-22.2.git HEAD
```

The local manifest uses the LineageOS `private` remote, which resolves through SSH.

## Sync/build

The normal build script performs the local-manifest copy, `repo sync`, patch application and build.

Target for the P11 reference variant:

```bash
bash lineage_build_unified/build_unified.sh treble 64GN
```

Expected target:

```text
gsi_arm64_gN
```

If `$HOME/.android-certs` does not exist, the script will continue without producing the same signed form as Andy Yan's published image. Andy Yan's original signing keys are not part of this experiment.

## Verify pinned sources before spending time on the build

After the sync/patch preparation has populated the source tree, run:

```bash
bash lineage_build_unified/verify_p11_magic_trackpad.sh
```

Expected result:

```text
OK: Trebuchet Magic Trackpad
OK: Andy Yan GSI device tree
OK: QcRilAm
OK: TrebleDroid hardware overlays
OK: Andy Yan LineageOS 22 Light patches
All pinned P11 sources match.
```

A mismatch is a stop condition. Do not proceed to physical validation until the source discrepancy is understood.

## First physical validation

The first build is accepted only if both the baseline functions and the Trackpad customization behave correctly.

### Wi-Fi baseline

- Wi-Fi can be enabled.
- Available networks are discovered.
- The previously saved SSID is visible when in range.
- The device reconnects to the saved network.
- Network traffic works after connection.

A failure to discover any networks is a build/base regression and must be investigated separately from the Trebuchet gesture code.

### Bluetooth / Trackpad baseline

- Magic Trackpad pairs.
- Pointer movement works.
- Click works.
- Existing two-finger behavior remains usable.
- Existing three-finger behavior remains usable.
- Existing horizontal four-finger behavior is not replaced by the custom vertical action.

### Custom gestures

- Four-finger vertical swipe up opens All Apps.
- Four-finger vertical swipe down returns Home.
- Small diagonal noise does not incorrectly convert a horizontal gesture into a vertical action.
- Cold-start All Apps works when Trebuchet is not already alive.

## Promotion rule

Do not merge the Trackpad customization into `main` before the physical validation above is accepted.

If Trackpad passes and Wi-Fi remains healthy, the Trackpad commit can be promoted independently.

If Trackpad passes but Wi-Fi fails, preserve the Trackpad commit as-is and investigate the GSI/platform reproduction separately. Do not add a speculative Wi-Fi workaround to the Trebuchet branch.
