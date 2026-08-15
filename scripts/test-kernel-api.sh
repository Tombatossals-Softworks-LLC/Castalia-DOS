#!/usr/bin/env bash
# =====================================================================
#  test-kernel-api.sh  -  prove the Castalia kernel API on an emulated PC
# ---------------------------------------------------------------------
#  Boots the built floppy image in QEMU - a whole-PC emulator running our
#  real KERNEL.SYS - runs KTEST.EXE from AUTOEXEC.BAT, and reads its
#  report back out of the image.
#
#  Why QEMU and not the DOSBox boot test we already have: DOSBox's `boot`
#  does start our kernel, but DOSBox is a game-focused emulator and its
#  DOS-level behaviour is its own.  QEMU emulates the machine and nothing
#  else, so what answers INT 2Fh here is unambiguously our kernel.  It is
#  still not metal - see docs/TESTING.md for what only real hardware can
#  settle (timing, Sound Blaster IRQ/DMA, real IDE quirks) - but it turns
#  "the kernel patch compiles" into "the kernel patch runs".
#
#  Asserts, per docs/KERNEL.md:
#    CA00h -> installed, edition 01, OEM id CA
#    CA01h -> the profile CONFIG.SYS declared with CASTALIA=
#    CA02h -> a boot tick was stamped (non-zero)
#    INT 21h AH=30h -> BH carries the Castalia OEM id CAh
#
#  Usage:   scripts/test-kernel-api.sh [image]   (default: dist/*.img)
#  Env:     BOOT_TIMEOUT=75   seconds to let the guest boot
#  Needs:   qemu-system-i386, mtools, build/ktest.exe.
#  Exit:    0 pass, 1 fail, 2 missing prerequisite.
# =====================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TIMEOUT="${BOOT_TIMEOUT:-75}"

err()  { printf 'ERR  %s\n' "$*" >&2; }
info() { printf '  %s\n' "$*"; }
ok()   { printf '  OK   %s\n' "$*"; }

for t in qemu-system-i386 mcopy mdir; do
    command -v "$t" >/dev/null 2>&1 || { err "missing tool: $t"; exit 2; }
done

IMG="${1:-}"
if [ -z "$IMG" ]; then
    IMG="$(find "$ROOT/dist" -maxdepth 1 -name '*.img' -type f 2>/dev/null |
           sort | head -1)"
fi
if [ -z "$IMG" ] || [ ! -f "$IMG" ]; then
    err "no floppy image; build one first"
    exit 2
fi
[ -f "$ROOT/build/ktest.exe" ] || { err "build/ktest.exe missing (wmake ktest)"; exit 2; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cp "$IMG" "$WORK/boot.img"
cd "$WORK"

#  The profile the image's own CONFIG.SYS declares is what CA01h must
#  report back.  Read it rather than hard-coding, so this test keeps
#  telling the truth if the floppy profile ever changes.
mcopy -o -i boot.img ::/CONFIG.SYS CONFIG.SYS 2>/dev/null || true
WANT_PROFILE="$(sed -n 's/^[[:space:]]*CASTALIA=\([0-9]\).*/\1/p' CONFIG.SYS | head -1)"
[ -n "$WANT_PROFILE" ] || { err "no CASTALIA= directive in the image's CONFIG.SYS"; exit 1; }
info "image declares boot profile $WANT_PROFILE"

#  Run the probe first thing in AUTOEXEC, before anything interactive.
mcopy -o -i boot.img "$ROOT/build/ktest.exe" ::/KTEST.EXE
mcopy -o -i boot.img ::/AUTOEXEC.BAT ae.bat
{ echo "@ECHO OFF"; echo "A:\\KTEST.EXE"; tail -n +2 ae.bat; } > ae2.bat
mcopy -o -i boot.img ae2.bat ::/AUTOEXEC.BAT

info "booting in QEMU (${TIMEOUT}s window; the guest never halts, so the kill is expected)"
timeout "$TIMEOUT" qemu-system-i386 \
    -M isapc -cpu 486 -m 4 \
    -drive file=boot.img,format=raw,if=floppy \
    -boot a -display none -no-reboot >/dev/null 2>&1 || true

if ! mcopy -o -i boot.img ::/KTEST.TXT KTEST.RAW 2>/dev/null || [ ! -s KTEST.RAW ]; then
    err "the probe wrote nothing - the guest did not reach AUTOEXEC"
    exit 1
fi
#  DOS wrote this file, so every line ends CRLF.  Strip the CR before
#  matching: a "$" anchor never matches with one still attached, and a
#  test that can only fail is as useless as one that can only pass.
tr -d '\r' < KTEST.RAW > KTEST.TXT

echo
echo "  --- what the kernel reported ---"
sed 's/^/    /' KTEST.TXT
echo

fails=0
check() { # description regex
    if grep -qE "$2" KTEST.TXT; then ok "$1"; else
        printf '  FAIL %s\n' "$1"; fails=$((fails + 1)); fi
}

check "CA00h answers, edition 01, OEM id CA" \
      '^CA00 OK build=[0-9]+ edition=01 oem=CA$'
check "CA01h reports the profile CONFIG.SYS declared ($WANT_PROFILE)" \
      "^CA01 OK profile=$WANT_PROFILE\$"
check "CA02h stamped a boot tick (non-zero)" \
      '^CA02 OK boottick=[0-9]+$'
check "INT 21h AH=30h carries the Castalia OEM id in BH" \
      '^INT21\.30 major=[0-9]+ minor=[0-9]+ oem\(bh\)=CA$'

if grep -qE '^CA02 OK boottick=0$' KTEST.TXT; then
    printf '  FAIL the boot tick is zero - signon() did not stamp it\n'
    fails=$((fails + 1))
fi

echo
if [ "$fails" -eq 0 ]; then
    ok "KERNEL API TEST PASSED - the Castalia kernel answered every call"
    exit 0
fi
err "$fails kernel API check(s) failed"
exit 1
