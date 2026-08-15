#!/usr/bin/env bash
# =====================================================================
#  test-boot.sh  -  E2E boot test: boot the floppy image in DOSBox
# ---------------------------------------------------------------------
#  Boots the built CASTALIA DOS floppy image in headless DOSBox and
#  asserts that the boot reached AUTOEXEC.BAT:
#
#    1. copy the image; inject A:\CITEST.FLG (the marker trigger)
#    2. boot it (SDL dummy video, no sound) under a timeout - once the
#       guest boots, the host cannot make it exit, so the kill is the
#       expected end (exit 124)
#    3. read A:\BOOTOK.TXT back off the image with mtools and assert
#       it says CASTALIA-BOOT-OK (written by the floppy AUTOEXEC.BAT
#       only when the flag file is present)
#
#  Usage:   scripts/test-boot.sh [image]        (default: dist/*.img)
#  Env:     BOOT_TIMEOUT=45   seconds to let the guest boot
#  Needs:   dosbox, mtools.   Exit 0 = booted, 1 = failed, 2 = missing.
# =====================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TIMEOUT="${BOOT_TIMEOUT:-45}"

IMG="${1:-}"
if [ -z "$IMG" ]; then
    # newest dist/*.img by modification time
    IMG="$(find "$ROOT/dist" -maxdepth 1 -name '*.img' -printf '%T@ %p\n' \
             2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-)"
fi
if [ -z "$IMG" ] || [ ! -f "$IMG" ]; then
    echo "ERR no image found (build one with scripts/build-floppy.sh)"
    exit 2
fi
for t in dosbox mcopy mdel; do
    command -v "$t" >/dev/null 2>&1 || { echo "ERR missing tool: $t"; exit 2; }
done

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cp "$IMG" "$WORK/boot.img"
cd "$WORK"

echo "  image: $IMG"
printf 'ci' > CITEST.FLG
mcopy -o -i boot.img CITEST.FLG ::/CITEST.FLG
mdel -i boot.img ::/BOOTOK.TXT 2>/dev/null || true

cat > boot.conf <<'EOF'
[sdl]
output=surface
[mixer]
nosound=true
[autoexec]
boot boot.img -l a
EOF

echo "  booting in headless DOSBox (${TIMEOUT}s window)..."
set +e
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    timeout "$TIMEOUT" dosbox -conf boot.conf >/dev/null 2>&1
rc=$?
set -e
echo "  dosbox ended with $rc (124 = timeout kill, expected)"

if mcopy -o -i boot.img ::/BOOTOK.TXT BOOTOK.TXT 2>/dev/null \
   && grep -q "CASTALIA-BOOT-OK" BOOTOK.TXT; then
    echo "  BOOT TEST PASSED: the image booted to AUTOEXEC.BAT."
    exit 0
fi
echo "  BOOT TEST FAILED: marker not written (boot did not complete)."
exit 1
