# P11 Magic Trackpad 2 — opt-in Bluetooth/UHID compatibility bridge

> P11 Wi-Fi rule: this bridge is the replacement for the custom-kernel Trackpad
> path. Build it with the `p11` profile and keep/restore the Lenovo stock boot
> kernel. The recompiled Trackpad kernel is known to cross the Wi-Fi regression
> boundary on this tablet.


Status (2026-10-02): **implementation and host tests complete; full Android build
and physical validation pending**. This is an experimental candidate, not a
claim that two/three/four-finger gestures have passed on the tablet.

## Why this route

The supplied `p11-kernel-baseline-20261002.tar.gz` establishes:

- Kernel `4.19.157-perf+`, built 2023-08-10 with Clang 10.0.7.
- Bluetooth device `004c:0265` binds to `hid-generic`.
- Input exposes relative mouse axes and ABS_MISC, without multitouch axes.
- Android loads Cursor/Joystick mappers, not TouchpadInputMapper.
- `CONFIG_HID_MAGICMOUSE=y`, `CONFIG_HID_MULTITOUCH=y`, `CONFIG_UHID=y`.
- `CONFIG_HIDRAW` is disabled. A hidraw daemon cannot use this stock kernel.
- Module signatures are enforced and module versioning is enabled. An arbitrary
  external replacement module cannot simply be inserted.
- Both partition-image checksums were verified:
  - boot_a.img: `1450bf03e8aa411f0940c3cda203d0f463a6fec55ef983ad5b19b257701e086f`
  - dtbo_a.img: `875955aadcf511b8bcea36106b791091976b05ec5b67c71e4fe8fac9c3673bbf`

Linux added MT2 Bluetooth support in commit
[`9d7b18668956c411a422d04c712994c5fdb23a4b`](https://kernel.googlesource.com/pub/scm/linux/kernel/git/hid/hid/+/9d7b18668956c411a422d04c712994c5fdb23a4b).
The publicly available Lenovo tree examined here lacks that MT2 support and has
a different kernel version. [Lenovo issue #1](https://github.com/lenovo/gplcc/issues/1)
also reports that the published 4.19.95 source did not boot a 4.19.157 device.
This is a compatibility warning, not proof that every custom kernel must fail.

The chosen candidate translates Bluetooth reports **before** they reach UHID.
It uses the already-enabled generic `hid-multitouch` driver. It needs no hidraw,
uinput permission changes, unsigned modules, kernel rebuild, or SELinux bypass.

## Scope and behavior

Default is OFF. Only newly created Bluetooth UHID devices whose physical VID/PID
is exactly `004c:0265` are eligible, and only when
`persist.bluetooth.p11_mt2_bridge=true`. Other devices pass through unchanged.
The real VID/PID, Bluetooth address, name and connection lifecycle remain intact.

For an enabled MT2 connection the implementation:

1. Replaces its UHID report descriptor with a standard Digitizer/Touch Pad
   descriptor containing 16 slots, button, coordinates, pressure and touch area.
2. Answers the synthetic Contact Count Maximum feature locally.
3. Sends the actual device feature report `F1 02 01` on UHID_START to enable MT.
4. Decodes Bluetooth input report `31`, with a four-byte prefix followed by
   nine-byte contacts, using the protocol documented in upstream Linux.
5. Emits a complete standard HID frame containing every slot, so an omitted or
   lifted finger cannot remain stuck. The fixed contact count includes inactive
   IDs; tip switch determines which contacts are touching.
6. Drops relative mouse/other input reports for this connection to avoid a
   duplicate pointer stream. Unsupported synthetic feature requests are rejected
   locally rather than forwarded under the real device's report numbering.

The generic kernel driver should then expose ABS_MT_SLOT/POSITION/TRACKING_ID/
PRESSURE and INPUT_PROP_POINTER/BUTTONPAD. Android, rather than the bridge,
interprets gestures; the existing Trebuchet four-finger code remains pinned.

This Bluetooth-only candidate does not implement USB transport, haptic settings,
battery reporting or orientation of the contact ellipse. Reconnection reapplies
the mode request; behavior across suspend/resume still requires physical testing.
If the mode request fails, enabling the bridge can temporarily remove cursor
motion; disabling the property and reconnecting restores the original path.

## Apply and build on the existing workstation

Keep the known-working `system.img.gz` as a rollback copy; do not overwrite it
with this experiment before acceptance. The previous image SHA256 is
`34a7ed33ac197e5e95f8baff758170616d6f56d29894521e08a0b01049d223f8`.

From the existing Android workspace:

```bash
cd ~/lineage-22-build-gsi
git -C lineage_build_unified pull --ff-only origin experimental/p11-magic-trackpad
python3 lineage_build_unified/p11/mt2-bridge/apply.py "$PWD"
source build/envsetup.sh
source vendor/lineage/vars/aosp_target_release
lunch "lineage_gsi_arm64_gN-${aosp_target_release}-userdebug"
WITH_ADB_INSECURE=true make -j4 systemimage
```

Stop if the patch script reports STOP or lunch fails. The patch script accepts a
clean contextual match, is idempotent, and refuses a changed bridge header. It
does not sync repositories, apply other patches, clean out/, alter manifests,
or touch Trebuchet. The normal build_unified.sh is not run for this increment.

The result should be `out/target/product/lineage_gsi_arm64/system.img`.
Check the successful build output before planning installation. Do not flash
boot, dtbo, vendor, or wipe userdata for this bridge. No automated flash is
included here. Full Soong compilation cannot be claimed from the host tests.

## Enable only after installing the rebuilt GSI

After confirming normal Bluetooth/cursor operation with the default-off image:

```bash
adb root
adb wait-for-device
adb shell setprop persist.bluetooth.p11_mt2_bridge true
```

Turn the Magic Trackpad off and on to recreate its HID connection. Then collect:

```bash
adb shell getprop persist.bluetooth.p11_mt2_bridge
adb shell 'for d in /sys/bus/hid/devices/*004C:0265*; do readlink -f "$d/driver"; done'
adb shell getevent -lp
adb shell dumpsys input
adb logcat -d -s bt_stack
```

Look for `P11 MT2: standard multitouch descriptor enabled` and
`P11 MT2: requested Bluetooth multitouch mode F1 02 01` in Bluetooth logs.
The driver should be `hid-multitouch`, not `hid-generic` or `magicmouse`.

Physical acceptance, in order:

- Wi-Fi still scans/connects; other paired input devices work.
- Pointer and click work with bridge enabled.
- MT axes, pressure and pointer property appear in getevent.
- TouchpadInputMapper appears in dumpsys input.
- Two-finger scrolling works; test zoom in an app supporting it.
- Three-finger navigation works.
- Four-finger up opens All Apps; down returns Home.
- Lift all fingers: no stuck contact or continuing gesture.
- Disconnect/reconnect, suspend/resume and reboot preserve operation.

If the MT mapper works but navigation does not, capture input/gesture logs before
changing Trebuchet. Do not assume that adding MT axes guarantees every gesture.

Immediate runtime rollback (then turn the trackpad off/on):

```bash
adb shell setprop persist.bluetooth.p11_mt2_bridge false
```

Source rollback, if required, is supported with `apply.py <android-root> --revert`;
it reverses only this integration patch and removes its matching header. It does
not roll back an already installed image without a rebuild/reinstallation.

## Validation and source provenance

Tests run successfully in Work:

- C++17 host compilation with `-Wall -Wextra -Werror`, ASan and UBSan.
- Sparse IDs and four fingers; all signed 13-bit coordinates; range clamping;
  pressure/area; empty frames, releases, duplicates, truncation and foreign reports.
- 20,000 deterministic malformed/random inputs under sanitizers.
- Per-fd opt-in, unrelated devices, destroy/recreate, feature replies.
- Actual patched UHID writer against pipes, including the short heap allocations
  used by Android's input queue, and byte-identical unrelated reports.
- Apply twice, check, exact reverse, refusal on incompatible source.
- HID parser from `hid-tools`: Touch Pad application; input report 179 bytes;
  feature report two bytes; axis physical units. Separate check confirms 115
  input main fields, below the kernel limit of 128.

LeakSanitizer was disabled because the execution sandbox prevents its /proc/task
inspection; AddressSanitizer and UndefinedBehaviorSanitizer remained enabled.
No physical reports beyond the supplied diagnostic snapshot were available.

Run the self-contained tests with `bash p11/mt2-bridge/tests/run.sh`. Run
`tests/integration_test.py <pristine-bta_hh_co.cc>` for the actual writer test.

Audited Android source:

- [LineageOS Bluetooth lineage-22.2](https://github.com/LineageOS/android_packages_modules_Bluetooth/tree/70aa5d56f2ca8173a506e18ea2099f577a846318)
- `system/btif/co/bta_hh_co.cc` SHA256:
  `05981d10548ee1506c14fae2f83140ad89625dcaacbfc3e23d3a067eec6620d9`
- Integration covers both enabled and disabled `hid_report_queuing` paths by
  translating only at the real UHID fd boundary.

Kernel reference examined for generic HID classification/multitouch handling:
[JulianDroske source, commit 88b3ba5](https://github.com/JulianDroske/linux_kernel_lenovo_tbj606f/tree/88b3ba552eae9f322e1c0fcbf14ea3de5d5976e8).
This is **not** asserted to be the exact source of the binary baseline.

The bridge is an independent Apache-2.0 implementation of the device protocol;
the Linux driver is a research reference (Claudio Mettler, Marek Wyborski,
Sean O'Brien and other upstream contributors). No Linux driver file is copied
into the Android build. The baseline archive and its personal diagnostics are
not committed to this repository.
