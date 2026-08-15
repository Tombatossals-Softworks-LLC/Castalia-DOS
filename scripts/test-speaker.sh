#!/usr/bin/env bash
# =====================================================================
#  test-speaker.sh  -  the PC speaker, measured rather than assumed
# ---------------------------------------------------------------------
#  Runs build/spktest.exe inside DOSBox-X with SDL's disk audio driver
#  capturing everything the emulated speaker produces, then measures the
#  captured samples: four bursts, at the four frequencies the program
#  asked for, and silence afterwards.
#
#  Why this exists: tests/unit/test_spk.c covers SPK's state machine
#  thoroughly, but on the host its port writes compile to no-ops.  A
#  divisor computed from the wrong clock constant, or the gate bits set
#  in the wrong order, passes every one of those tests and then plays
#  the wrong note - or nothing - on a real 386SX.  Sounding the tones
#  and measuring them is the only thing that settles it.
#
#  Leaving the speaker silent on exit is a stated rule in SPK.H, so the
#  tail of the capture is asserted silent too.
#
#  Needs: dosbox-x, python3, build/spktest.exe (wmake spktest).
#  Exit 0 = all green, 1 = a measurement disagreed, 2 = prerequisites.
# =====================================================================
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT" || exit 2

command -v dosbox-x >/dev/null 2>&1 || { echo "ERR dosbox-x not found"; exit 2; }
command -v python3  >/dev/null 2>&1 || { echo "ERR python3 not found"; exit 2; }
[ -f build/spktest.exe ] || { echo "ERR build/spktest.exe missing (run: wmake spktest)"; exit 2; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cp build/spktest.exe "$WORK/SPKTEST.EXE"

cat > "$WORK/dosbox.conf" <<EOF
[sdl]
output=surface
[dosbox]
machine=svga_s3
[mixer]
nosound=false
rate=44100
blocksize=1024
[speaker]
pcspeaker=true
pcrate=44100
[autoexec]
mount c $WORK
c:
SPKTEST.EXE
exit
EOF

echo "== sounding the tones in DOSBox-X, capturing the speaker =="
( cd "$WORK" && \
  SDL_VIDEODRIVER=dummy \
  SDL_AUDIODRIVER=disk \
  SDL_DISKAUDIOFILE="$WORK/audio.raw" \
  timeout 180 dosbox-x -conf "$WORK/dosbox.conf" -nolog -fastlaunch >/dev/null 2>&1 )

[ -s "$WORK/audio.raw" ] || { echo "ERR no audio was captured"; exit 1; }
[ -f "$WORK/SPKTEST.TXT" ] || { echo "ERR SPKTEST.EXE did not finish"; exit 1; }

python3 - "$WORK/audio.raw" <<'PY'
import math, struct, sys

SR       = 44100.0          # matches [mixer] rate in the config above
WANT     = [220, 440, 880, 1760]
TOL_PCT  = 2.0              # frequency tolerance
MIN_LEN  = 0.30             # each tone is 8 BIOS ticks, ~0.44 s
MAX_LEN  = 0.60
WIN      = 882              # 20 ms RMS window
LOUD     = 500.0            # RMS above this counts as "sounding"

raw   = open(sys.argv[1], 'rb').read()
n     = len(raw) // 2
frames= struct.unpack('<%dh' % n, raw[:n * 2])
left  = frames[0::2]        # stereo s16; one channel is enough

fails = 0
def ck(cond, what):
    global fails
    print("  [ OK ] " + what if cond else "  [FAIL] " + what)
    if not cond:
        fails += 1

# --- find the bursts -------------------------------------------------
env = []
for i in range(0, len(left) - WIN, WIN):
    seg = left[i:i + WIN]
    env.append(math.sqrt(sum(float(x) * x for x in seg) / len(seg)))

groups, cur = [], []
for i, v in enumerate(env):
    if v > LOUD:
        cur.append(i)
    elif cur:
        groups.append(cur); cur = []
if cur:
    groups.append(cur)
# ignore anything shorter than 60 ms: mixer start-up can click
groups = [g for g in groups if len(g) * WIN / SR > 0.06]

print("== speaker output measured from the captured samples ==")
ck(len(groups) == len(WANT),
   "%d tones sounded (expected %d)" % (len(groups), len(WANT)))

# --- measure each one ------------------------------------------------
def goertzel(seg, f):
    k = 2.0 * math.cos(2.0 * math.pi * f / SR)
    s1 = s2 = 0.0
    for x in seg:
        s0 = x + k * s1 - s2
        s2, s1 = s1, s0
    return math.sqrt(abs(s1 * s1 + s2 * s2 - k * s1 * s2))

for idx, want in enumerate(WANT):
    if idx >= len(groups):
        ck(False, "tone %d (%d Hz) was never sounded" % (idx + 1, want))
        continue
    g     = groups[idx]
    start = g[0] * WIN
    dur   = len(g) * WIN / SR
    ck(MIN_LEN <= dur <= MAX_LEN,
       "tone %d lasts %.2f s (expected %.2f-%.2f)" % (idx + 1, dur, MIN_LEN, MAX_LEN))

    # skip the attack and release; analyse the steady middle
    a   = start + int(0.06 * SR)
    seg = left[a:a + 4096]
    best_f, best_m = 0.0, -1.0
    f = 150.0
    while f < 2600.0:
        m = goertzel(seg, f)
        if m > best_m:
            best_f, best_m = f, m
        f += 2.0
    err = abs(best_f - want) / want * 100.0
    ck(err <= TOL_PCT,
       "tone %d measures %.0f Hz, asked for %d Hz (%.2f%% off)"
       % (idx + 1, best_f, want, err))

# --- the speaker must be silent when the program exits ---------------
tail = env[-5:] if len(env) >= 5 else env
ck(all(v < LOUD for v in tail),
   "speaker is silent after the program exits (SPK.H's rule)")

print("== %s ==" % ("speaker tests PASSED" if fails == 0 else
                    "speaker tests FAILED (%d)" % fails))
sys.exit(1 if fails else 0)
PY
