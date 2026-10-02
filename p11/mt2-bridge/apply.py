#!/usr/bin/env python3
"""Apply/revert only the P11 MT2 bridge to an existing Android build tree."""
import argparse
from pathlib import Path
import subprocess
import sys


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("android_root", type=Path)
    mode = p.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--revert", action="store_true")
    args = p.parse_args()
    assets = Path(__file__).resolve().parent
    root = args.android_root.resolve() / "packages/modules/Bluetooth"
    source = root / "system/btif/co/bta_hh_co.cc"
    header = source.parent / "p11_mt2_bridge.h"
    patch = assets / "bluetooth.patch"
    wanted = (assets / header.name).read_bytes()
    if not source.is_file():
        sys.exit(f"STOP: Bluetooth source not found: {source}")
    if header.exists() and header.read_bytes() != wanted:
        sys.exit(f"STOP: differing bridge header already exists: {header}; preserve it and review the diff")

    def git_apply(*flags):
        return subprocess.run(["git", "apply", *flags, str(patch)], cwd=root,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    forward = git_apply("--check")
    reverse = git_apply("--reverse", "--check")
    if args.revert:
        if reverse.returncode:
            sys.exit("STOP: patch is not cleanly reversible; no files changed\n" + reverse.stderr)
        result = git_apply("--reverse")
        if result.returncode:
            sys.exit(result.stderr)
        if header.exists():
            header.unlink()
        print("Bridge reverted. Rebuild to remove it from the installed GSI.")
        return

    if reverse.returncode == 0:
        if not args.check and not header.exists():
            header.write_bytes(wanted)
        elif args.check and not header.exists():
            sys.exit("STOP: patch present but bridge header missing")
        print("OK: P11 MT2 bridge already applied" if header.exists() else "STOP: missing header")
        return
    if forward.returncode:
        sys.exit("STOP: source differs from the audited LineageOS 22.2 integration; no files changed\n"
                 + forward.stderr)
    if args.check:
        print("OK: Bluetooth integration patch applies cleanly")
        return
    # Write the new header first: it is unused until the source patch succeeds.
    existed = header.exists()
    header.write_bytes(wanted)
    result = git_apply()
    if result.returncode:
        if not existed:
            header.unlink()
        sys.exit(result.stderr)
    print("Applied P11 MT2 bridge. Default OFF; rebuild systemimage before enabling on the tablet.")


if __name__ == "__main__":
    main()
