#!/usr/bin/env bash
# =====================================================================
#  test-video.sh  -  VIDDET against every adapter generation
# ---------------------------------------------------------------------
#  Runs build/vidtest.exe inside DOSBox-X once per emulated adapter and
#  asserts the shared VIDDET module names each one correctly.
#
#  Why this exists: HWINFO used to ask a single question - INT 10h
#  AX=1A00h, "get display combination code" - and answer "VGA present"
#  or "not detected (EGA/CGA?)".  The reference 386SX reported the
#  second, which is both unhelpful and unverifiable: QEMU has no CGA or
#  EGA to test the other branches against.  DOSBox-X does, via its
#  machine= setting, so the branches that used to be untested are
#  assertions now.
#
#  Needs: dosbox-x and build/vidtest.exe (wmake vidtest).
#  Exit 0 = all green, 1 = a mismatch, 2 = prerequisites missing.
# =====================================================================
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT" || exit 2

command -v dosbox-x >/dev/null 2>&1 || { echo "ERR dosbox-x not found"; exit 2; }
[ -f build/vidtest.exe ] || { echo "ERR build/vidtest.exe missing (run: wmake vidtest)"; exit 2; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cp build/vidtest.exe "$WORK/VIDTEST.EXE"

fails=0
pass() { printf '  [ OK ] %s\n' "$*"; }
bad()  { printf '  [FAIL] %s\n' "$*"; fails=$((fails+1)); }

# machine= value : the VIDDET class name it must report
CASES="
mda|MDA (monochrome)
hercules|MDA (monochrome)
cga|CGA
tandy|CGA
pcjr|CGA
amstrad|CGA
ega|EGA
mcga|MCGA (no EGA modes)
vgaonly|VGA or better
svga_s3|VGA or better
vesa_nolfb|VGA or better
"

run_case() {
    machine="$1"
    want="$2"
    rm -f "$WORK/VIDTEST.TXT"
    conf="$WORK/dosbox-$machine.conf"
    cat > "$conf" <<EOF
[sdl]
output=surface
[dosbox]
machine=$machine
[mixer]
nosound=true
[autoexec]
mount c $WORK
c:
VIDTEST.EXE
exit
EOF
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
        timeout 90 dosbox-x -conf "$conf" -nolog -fastlaunch >/dev/null 2>&1

    if [ ! -f "$WORK/VIDTEST.TXT" ]; then
        bad "machine=$machine: VIDTEST.TXT was not produced"
        return
    fi
    got="$(sed 's/\r$//' "$WORK/VIDTEST.TXT" | sed -n 's/^CLASS [0-9]* //p')"
    raw="$(sed 's/\r$//' "$WORK/VIDTEST.TXT" | tr '\n' ' ')"
    if [ "$got" = "$want" ]; then
        pass "machine=$machine -> $got"
    else
        bad "machine=$machine -> '$got', expected '$want'"
        printf '         probes: %s\n' "$raw"
    fi
}

echo "== VIDDET against each emulated adapter (DOSBox-X) =="
echo "$CASES" | while IFS='|' read -r m w; do
    [ -n "$m" ] || continue
    run_case "$m" "$w"
done > "$WORK/out.txt" 2>&1
cat "$WORK/out.txt"
fails="$(grep -c '\[FAIL\]' "$WORK/out.txt")"

if [ "$fails" -eq 0 ]; then
    echo "== video detection tests PASSED =="
    exit 0
fi
echo "== video detection tests FAILED ($fails mismatch(es)) =="
exit 1
