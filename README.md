# LineageOS 22 Light — P11 Magic Trackpad experiment

This branch is a controlled Lenovo Tab P11 experiment based on Andy Yan's `lineage-22-light` build recipe.

**2026-10-02 update:** the first GSI built and booted; Wi-Fi and pointer work, but
2/3/4-finger gestures fail because the stock kernel exposes no MT2 multitouch.
An opt-in Bluetooth/UHID bridge is now available in
[p11/mt2-bridge](p11/mt2-bridge/README.md). It has host tests; Android compilation
and physical acceptance remain pending. Follow its incremental instructions for
the existing build tree. It is not automatically applied by the full build recipe.

The original build recipe's functional customization is the replacement of LineageOS Trebuchet with the pinned Magic Trackpad implementation from:

```text
remappingbridge/trebuchet-lineage-22.2
96ddd23a6400962798c6d119ef165594425137f8
```

Do **not** add Wi-Fi workarounds to this branch before the first physical build. Wi-Fi is being used as a baseline regression check.

Full source pins, build commands, known reproducibility limitations and the physical acceptance checklist are documented in:

```text
P11_MAGIC_TRACKPAD.md
```

## Workspace

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

The custom Trebuchet repository is private. Confirm GitHub SSH access before syncing:

```bash
git ls-remote git@github.com:remappingbridge/trebuchet-lineage-22.2.git HEAD
```

## Target

For the P11 GApps/no-root ARM64 GSI:

```bash
bash lineage_build_unified/build_unified.sh treble 64GN
```

Before physical validation, verify the pinned repositories:

```bash
bash lineage_build_unified/verify_p11_magic_trackpad.sh
```

See `P11_MAGIC_TRACKPAD.md` before building.
