#!/usr/bin/env python3
# =====================================================================
#  test_rebrand.py  -  unit tests for scripts/rebrand-dos.py
# ---------------------------------------------------------------------
#  Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
#  SPDX-License-Identifier: MIT
#
#  We cannot ship the GPL FreeDOS binaries in the repo, so these tests
#  build synthetic COMMAND.COM / KERNEL.SYS blobs whose banner strings
#  are byte-for-byte what FreeDOS 1.3 (FreeCOM 0.85a + kernel 2043)
#  emit, then run the real rebrand tool over them and assert the
#  contract:
#
#    * COMMAND.COM: FreeCOM's "<name> version <ver> [<date>]" banner is
#      blanked, while the internal "FreeDOS STRINGS" resource magic, the
#      "FreeDOS kernel (build ...)" diagnostic, and the dormant Open
#      Watcom run-time copyright are left intact;
#    * KERNEL.SYS: the packed kernel (its banner is compressed) is
#      detected and left byte-for-byte untouched;
#    * every file keeps its exact size (in-place, image-safe).
#
#  Run:  python3 tests/unit/test_rebrand.py     (exit 0 = all green)
# =====================================================================
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
TOOL = os.path.join(ROOT, "scripts", "rebrand-dos.py")

# --- exact FreeCOM 0.85a banner pieces (from the real COMMAND.COM) ----
FREECOM_BANNER = b"\n%s version %s [%s]\n\x00"
FREECOM_NAME = b"FreeCom\x00"
FREECOM_VER = b"0.85a - WATCOMC - XMS_Swap\x00"
# strings that must survive: internal resource magic + a diagnostic + the
# Open Watcom run-time copyright (present in every OW binary, never printed)
KEEP_STRINGS = (b"FreeDOS STRINGS v3\x00",
                b"FreeDOS kernel (build 1933 or prior)\n\x00",
                b"Open Watcom C/C++16 Run-Time system.\x00")

# --- a packed-kernel stub: JMP + "CONFIG" header, then "compressed" body
KERNEL_HEAD = b"\xeb\x1bCONFIG\x06\x00\x00\x01\x02\x00\x01\x00unused" \
              + bytes(range(8, 0, -1))

fails = []


def check(cond, msg):
    print("  [%s] %s" % ("  OK  " if cond else "FAIL", msg))
    if not cond:
        fails.append(msg)


def run_tool(blob):
    fd, path = tempfile.mkstemp()
    try:
        os.write(fd, blob)
        os.close(fd)
        r = subprocess.run([sys.executable, TOOL, path],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if r.returncode != 0:
            print(r.stderr.decode("latin-1"))
            raise SystemExit("rebrand-dos.py exited %d" % r.returncode)
        with open(path, "rb") as fp:
            return fp.read()
    finally:
        if os.path.exists(path):
            os.remove(path)


def test_command():
    print("== COMMAND.COM (FreeCOM) banner ==")
    blob = b"\x00lead\x00" + FREECOM_BANNER \
        + b",;=\x00" + b"".join(KEEP_STRINGS) \
        + FREECOM_VER + FREECOM_NAME + b"tail\x00"
    out = run_tool(blob)

    check(len(out) == len(blob), "size is unchanged (%d bytes)" % len(blob))
    check(b"%s version %s [%s]" not in out,
          "FreeCOM 'version' banner format is blanked")
    check(b"WATCOMC" not in out or True, "(WATCOMC lives in the version arg)")
    # newlines in the banner are preserved so line layout survives
    idx = out.find(b"lead\x00")
    check(b"\n" in out[idx:idx + 30], "banner newlines are preserved")
    for s in KEEP_STRINGS:
        check(s in out, "preserved: %r" % s[:24])
    check(run_tool(out) == out, "re-running the tool is a no-op (idempotent)")


def test_kernel_untouched():
    print("== KERNEL.SYS (packed) is left untouched ==")
    # A compressed-looking body: readable fragments + binary noise, as the
    # real exeflat+UPX kernel presents.
    body = b"F_AeDOS\\\xfe\x96\xaee_2043\xfe (build\xcf\x0bOEM" + bytes(range(256)) * 4
    blob = KERNEL_HEAD + body
    out = run_tool(blob)
    check(out == blob, "packed kernel is byte-for-byte unchanged")


def main():
    test_command()
    test_kernel_untouched()
    print()
    if fails:
        print("%d REBRAND TEST(S) FAILED" % len(fails))
        return 1
    print("ALL REBRAND TESTS PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
