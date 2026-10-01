#!/usr/bin/env bash
# =====================================================================
#  verify-exes.sh  -  assert every Castalia tool was built and is a
#  real DOS MZ executable (first two bytes "MZ")
# =====================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

TOOLS=(castalia launch hwinfo setsound memprof setup safeboot cfgedit
       gamecfg castfm castmark castcopy castdoc snake puzzle almena
       minas banner help castedit castid siege reversi barrels
       solitare cdplayer saver casttour castlink undel
       smoke ktest vidtest spktest cdtest)

fails=0
for t in "${TOOLS[@]}"; do
    f="$ROOT/build/$t.exe"
    if [ ! -f "$f" ]; then
        echo "  FAIL $t.exe missing"
        fails=$((fails + 1))
    elif [ "$(head -c 2 "$f")" != "MZ" ]; then
        echo "  FAIL $t.exe is not an MZ executable"
        fails=$((fails + 1))
    else
        echo "  OK   $t.exe ($(stat -c %s "$f") bytes)"
    fi
done
[ "$fails" -eq 0 ] && { echo "all executables verified."; exit 0; }
echo "$fails executable(s) missing or invalid."
exit 1
