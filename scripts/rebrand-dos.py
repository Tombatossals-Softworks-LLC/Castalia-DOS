#!/usr/bin/env python3
# =====================================================================
#  rebrand-dos.py  -  strip the FreeDOS / Open Watcom sign-on text from
#  the fetched DOS binaries so a CASTALIA DOS boot shows ONLY Castalia.
# ---------------------------------------------------------------------
#  Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
#  SPDX-License-Identifier: MIT
#
#  WHAT PRINTS AT BOOT, AND WHAT WE CAN TOUCH
#  ------------------------------------------
#  Two FreeDOS components print a sign-on at boot:
#
#    * KERNEL.SYS - "FreeDOS kernel <v> ... WATCOMC ...", then the
#      Villani/GPL copyright.  In the FreeDOS 1.3 kernel (build 2043)
#      signon() prints this UNCONDITIONALLY, and the kernel image is
#      packed (exeflat + UPX --8086), so those strings are COMPRESSED and
#      cannot be byte-patched in place.  Removing that banner needs a
#      kernel rebuilt from source (see docs/BOOT.md); this tool only
#      detects the packed kernel and reports it, it does not touch it.
#
#    * COMMAND.COM (FreeCOM) - its "<name> version <ver> [<date>]" line,
#      where <ver> carries "WATCOMC".  FreeCOM is NOT packed, so we blank
#      that one banner format string here (leaving the internal "FreeDOS
#      STRINGS" resource magic and the dormant Open Watcom C run-time
#      copyright untouched - the latter is never printed and lives in
#      every Open-Watcom-built binary, ours included).
#
#  FreeDOS's copyright, GPL text, and warranty stay in the shipped source
#  (third_party/), in LICENSES/, and in the Castalia About screen; see
#  docs/LICENSE-STRATEGY.md 2.3.2a.
#
#  The patch is in place and length-preserving (image-safe), idempotent,
#  and graceful (a banner that is not found is a warning, not a guess).
#
#  Usage:  rebrand-dos.py FILE [FILE ...]
#  Exit:   0 on success (including graceful no-ops), 1 on a real error.
# =====================================================================
import sys

# The FreeCOM banner format string.  Blanking its text (keeping newlines)
# turns the "FreeCom version 0.85a - WATCOMC - XMS_Swap [date]" line into
# a blank line: with no %s specifiers left, FreeCOM's printf simply
# ignores the name/version/date arguments.
FREECOM_BANNER_FMT = b"%s version %s [%s]"

# Kernel signature: the .SYS starts with a short JMP over the patchable
# "CONFIG" block (EB xx 'CONFIG').
def looks_like_kernel(buf):
    return len(buf) > 8 and buf[0] == 0xEB and buf[2:8] == b"CONFIG"


def warn(msg):
    sys.stderr.write("  rebrand: WARN %s\n" % msg)


def info(msg):
    sys.stdout.write("  rebrand: %s\n" % msg)


def cstr_span(buf, anchor, start=0):
    """(str_start, nul_index) of the NUL-terminated C string containing
    `anchor`, or None.  C literals are NUL-separated, so walking back to
    the previous NUL recovers the true start."""
    i = buf.find(anchor, start)
    if i < 0:
        return None
    s = buf.rfind(b"\x00", 0, i)
    s = s + 1 if s >= 0 else 0
    e = buf.find(b"\x00", i)
    if e < 0:
        e = len(buf)
    return (s, e)


def blank_keep_newlines(b, s, e):
    """Blank buf[s:e] to spaces in place, but keep CR/LF so line breaks
    (and thus the surrounding layout) survive."""
    for k in range(s, e):
        if b[k] not in (0x0A, 0x0D):
            b[k] = 0x20


def patch_command(buf):
    """Blank FreeCOM's version banner.  Returns (new_bytes, [changes])."""
    b = bytearray(buf)
    changes = []
    span = cstr_span(b, FREECOM_BANNER_FMT)
    if not span:
        warn("COMMAND.COM: FreeCOM version banner not found "
             "(already blanked, or an unexpected shell build)")
        return bytes(b), changes
    s, e = span
    blank_keep_newlines(b, s, e)
    changes.append("FreeCOM version banner blanked")
    return bytes(b), changes


def detect_and_patch(path):
    with open(path, "rb") as fp:
        buf = fp.read()

    if FREECOM_BANNER_FMT in buf:
        new, changes = patch_command(buf)
        kind = "COMMAND.COM"
    elif looks_like_kernel(buf):
        # The kernel's boot banner is handled at the source, by the rebuild
        # in scripts/build-kernel.sh (the stock 1.3 kernel is packed and
        # prints its banner unconditionally, so it cannot be byte-patched).
        # Whatever KERNEL.SYS lands here, we leave it as-is.
        info("%s: kernel banner is handled by the source rebuild "
             "(scripts/build-kernel.sh) - left untouched" % path)
        return
    else:
        info("%s: no FreeDOS/FreeCOM boot banner; left untouched" % path)
        return

    if len(new) != len(buf):
        sys.stderr.write("  rebrand: ERROR %s changed size "
                         "(%d -> %d); refusing to write\n"
                         % (path, len(buf), len(new)))
        raise SystemExit(1)

    if new == buf:
        info("%s (%s): nothing to change" % (path, kind))
        return

    with open(path, "wb") as fp:
        fp.write(new)
    info("%s (%s): %s" % (path, kind, "; ".join(changes)))


def main(argv):
    if len(argv) < 2:
        sys.stderr.write("usage: rebrand-dos.py FILE [FILE ...]\n")
        return 1
    for path in argv[1:]:
        detect_and_patch(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
