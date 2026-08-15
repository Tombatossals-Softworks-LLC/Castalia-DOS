#!/usr/bin/env bash
# =====================================================================
#  test-dos.sh  -  run the DOS-side smoke test in headless DOSBox
# ---------------------------------------------------------------------
#  Executes build/smoke.exe (a real 16-bit DOS program built by Open
#  Watcom from tests/dos/SMOKE.C) inside DOSBox with the repo mounted
#  as C:, then asserts TESTOUT.TXT reports zero failures.
#
#  Needs: dosbox and build/smoke.exe (wmake smoke).
#  Exit 0 = all green, 1 = failures, 2 = prerequisites missing.
# =====================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

command -v dosbox >/dev/null 2>&1 || { echo "ERR dosbox not found"; exit 2; }
[ -f build/smoke.exe ] || { echo "ERR build/smoke.exe missing (run: wmake smoke)"; exit 2; }

rm -f TESTOUT.TXT
CONF="$(mktemp)"
trap 'rm -f "$CONF"' EXIT
cat > "$CONF" <<EOF
[sdl]
output=surface
[mixer]
nosound=true
[autoexec]
mount c $ROOT
c:
BUILD\\SMOKE.EXE
exit
EOF

echo "== running SMOKE.EXE inside headless DOSBox =="
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    timeout 60 dosbox -conf "$CONF" >/dev/null 2>&1 || true

[ -f TESTOUT.TXT ] || { echo "ERR TESTOUT.TXT was not produced"; exit 1; }
sed 's/\r$//' TESTOUT.TXT | sed 's/^/  /'

fails="$(sed 's/\r$//' TESTOUT.TXT | awk '/^RESULT/ {print $4}')"
rm -f TESTOUT.TXT
if [ -n "$fails" ] && [ "$fails" -eq 0 ] 2>/dev/null; then
    echo "== DOS smoke test PASSED =="
    exit 0
fi
echo "== DOS smoke test FAILED (failures: ${fails:-unknown}) =="
exit 1
