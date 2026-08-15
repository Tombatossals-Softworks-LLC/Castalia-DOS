#!/usr/bin/env bash
# =====================================================================
#  test-unit.sh  -  host unit tests (compile the real DOS sources
#  natively with gcc and run them)
# ---------------------------------------------------------------------
#  Covers the shared INI module (the parser every Castalia tool depends
#  on), the shared SPK beeper module, whose note/deadline state machine
#  is driven here by a fake clock so the midnight tick wrap and the mute
#  levers can be exercised without waiting for real time, and the
#  CASTLINK frame layer, which is driven against a virtual UART because
#  its null-modem protocol can never be tried on real hardware in CI -
#  round trip, header-covering CRC, duplicate suppression and the
#  alternating sequence bit are all asserted here or nowhere.
#  Needs only gcc.  Exit 0 = all green.
# =====================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

command -v gcc >/dev/null 2>&1 || { echo "ERR gcc not found"; exit 2; }

OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

echo "== building tests/unit/test_ini.c against src/common/INI.C =="
gcc -x c -std=c89 -Wall -Wextra -Werror -Isrc/common \
    -o "$OUT/test_ini" tests/unit/test_ini.c src/common/INI.C

"$OUT/test_ini"

echo
echo "== building tests/unit/test_spk.c against src/common/SPK.C =="
gcc -x c -std=c89 -Wall -Wextra -Werror -Isrc/common \
    -o "$OUT/test_spk" tests/unit/test_spk.c

"$OUT/test_spk"

echo
echo "== building tests/unit/test_castmark_scale.c (index + bar scale) =="
gcc -x c -std=c89 -Wall -Wextra -Werror -Isrc/common \
    -o "$OUT/test_castmark_scale" tests/unit/test_castmark_scale.c -lm
echo "== running test_castmark_scale =="
"$OUT/test_castmark_scale"

echo "== building tests/unit/test_castlink.c against src/castlink/CASTLINK.C =="
gcc -x c -std=c89 -Wall -Wextra -Werror -Dfar= -Dnear= -DCASTLINK_TEST \
    -Ici/stubs -Isrc/common \
    -o "$OUT/test_castlink" tests/unit/test_castlink.c

"$OUT/test_castlink"

echo
echo "== boot-banner rebrand tool (scripts/rebrand-dos.py) =="
if command -v python3 >/dev/null 2>&1; then
    python3 tests/unit/test_rebrand.py
else
    echo "  [skip] python3 not installed"
fi
