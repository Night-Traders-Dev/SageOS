#!/usr/bin/env python3
"""Report drift between the vendored arch trees and the arch/ submodules.

There are two copies of every architecture in this repository and only one of
them is built:

  src/arch/<arch>/   vendored plain files. This is what src/Makefile uses -- it
                     resolves `arch/$(ARCH)/linker.ld` and `boot.S` relative to
                     src/, so it lands here. 1533 of the 1587 tracked files in
                     this repo live under src/arch/.

  arch/<arch>/       git submodules pointing at SageOS_arm64, SageOS_rv64 and
                     SageOS_x64. Nothing in the build reads them.

They have already diverged, and the vendored side is ahead: src/arch/rv64/boot.S
carries the trap vector, S-mode delegation and the handoff-structure argument
that the submodule pin does not have. So "just use the submodules" is not
currently true, and deleting either side is not safe without reconciling first.

This script exists so the divergence cannot rot unnoticed. Run it from the
repository root:

    python3 check_arch_drift.py          # report
    python3 check_arch_drift.py --strict # exit 1 if any drift, for CI

Exit status is 0 when there is no drift, 1 when there is drift under --strict,
and 0 otherwise (so it is safe to run unconditionally).
"""

from __future__ import annotations

import argparse
import filecmp
import pathlib
import subprocess
import sys

ARCHES = ("arm64", "rv64", "x64")

# Files worth comparing. The vendored trees also carry a nested SageLang copy
# under arm64/core, which is not tracked and is not compared here.
COMPARE = ("boot.S", "linker.ld", "metal_shim.c")


def sh(*args: str, cwd: pathlib.Path) -> str:
    try:
        out = subprocess.run(
            args, cwd=cwd, capture_output=True, text=True, timeout=60
        )
        return out.stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--strict", action="store_true", help="exit 1 on any drift")
    args = ap.parse_args()

    root = pathlib.Path(__file__).resolve().parent
    any_drift = False

    for arch in ARCHES:
        vend = root / "src" / "arch" / arch
        sub = root / "arch" / arch
        print("== %s" % arch)

        if not vend.is_dir():
            print("   vendored tree missing: %s" % vend)
            any_drift = True
            continue
        if not sub.is_dir():
            print("   submodule not checked out: %s" % sub)
            print("   (run: git submodule update --init)")
            continue

        for name in COMPARE:
            v, s = vend / name, sub / name
            if not v.exists() and not s.exists():
                continue
            if v.exists() != s.exists():
                print("   %-14s only in %s" % (
                    name, "src/arch" if v.exists() else "arch/"))
                any_drift = True
                continue
            if filecmp.cmp(v, s, shallow=False):
                print("   %-14s identical" % name)
                continue

            diff = subprocess.run(
                ["diff", "-u", str(s), str(v)],
                capture_output=True, text=True,
            ).stdout.splitlines()
            changed = [l for l in diff if l[:1] in "+-" and l[:3] not in ("+++", "---")]
            print("   %-14s DIFFERS (%d lines; vendored is authoritative)"
                  % (name, len(changed)))
            for line in changed[:6]:
                print("        %s" % line)
            if len(changed) > 6:
                print("        ... %d more" % (len(changed) - 6))
            any_drift = True

        vend_sha = sh("git", "log", "-1", "--format=%h %ad", "--date=short",
                      "--", "src/arch/%s" % arch, cwd=root)
        sub_sha = sh("git", "-C", str(sub), "log", "-1", "--format=%h %ad",
                     "--date=short", cwd=root)
        if vend_sha and sub_sha:
            print("   vendored last change: %s" % vend_sha)
            print("   submodule HEAD:       %s" % sub_sha)
        print()

    if any_drift:
        print("drift present. src/arch/ is what src/Makefile builds, so it is the")
        print("side that matters; arch/ submodules are unused mirrors. Reconcile by")
        print("pushing src/arch/<arch> upstream, then re-pinning the submodule.")
    else:
        print("no drift: vendored trees match the arch/ submodules.")

    return 1 if (any_drift and args.strict) else 0


if __name__ == "__main__":
    raise SystemExit(main())
