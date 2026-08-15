#!/usr/bin/env bash
# =====================================================================
#  test-cdaudio.sh  -  CDPLAYER's MSCDEX layer against a real disc image
# ---------------------------------------------------------------------
#  Generates a three-track Red Book audio disc (CUE/BIN, one pure tone
#  per track), mounts it in DOSBox-X, runs build/cdtest.exe - which is
#  CDPLAYER's own driver layer, #included behind -DCDPLAYER_TEST - and
#  then checks two independent things:
#
#    1. the report CDTEST wrote: drive found, table of contents read,
#       track starts and lengths correct, play/Q-channel/stop accepted;
#    2. the audio DOSBox-X actually produced, captured through SDL's
#       disk audio driver and measured.  CDTEST asks for track 2, so the
#       tone that comes out must be track 2's.  That is what catches a
#       Red Book address computed wrongly: the player would happily
#       report success while playing the wrong track.
#
#  What this CANNOT reach: UIDE and SHSUCDX.  DOSBox-X provides MSCDEX
#  itself, so the driver stack below the API is not the one a real 386SX
#  uses.  Everything above the API - request headers, control blocks,
#  the Red Book arithmetic - is ours, and that is what is under test.
#
#  Needs: dosbox-x, python3, build/cdtest.exe (wmake cdtest).
#  Exit 0 = all green, 1 = a check failed, 2 = prerequisites missing.
# =====================================================================
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT" || exit 2

command -v dosbox-x >/dev/null 2>&1 || { echo "ERR dosbox-x not found"; exit 2; }
command -v python3  >/dev/null 2>&1 || { echo "ERR python3 not found"; exit 2; }
[ -f build/cdtest.exe ] || { echo "ERR build/cdtest.exe missing (run: wmake cdtest)"; exit 2; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cp build/cdtest.exe "$WORK/CDTEST.EXE"

# --- the disc: 3 audio tracks, 5 s each, one tone per track ----------
python3 - "$WORK" <<'PY'
import math, struct, sys
work = sys.argv[1]
SR, SECT, SECS = 44100, 2352, 5
def tone(f):
    out = bytearray()
    for i in range(SR * SECS):
        v = int(20000 * math.sin(2.0 * math.pi * f * i / SR))
        out += struct.pack('<hh', v, v)
    while len(out) % SECT:
        out += b'\x00'
    return bytes(out)
parts = [tone(440), tone(880), tone(220)]
open(work + '/test.bin', 'wb').write(b''.join(parts))
def msf(s):
    return "%02d:%02d:%02d" % (s // (75 * 60), (s // 75) % 60, s % 75)
cue, start = 'FILE "test.bin" BINARY\n', 0
for i, p in enumerate(parts, 1):
    cue += '  TRACK %02d AUDIO\n    INDEX 01 %s\n' % (i, msf(start))
    start += len(p) // SECT
open(work + '/test.cue', 'w').write(cue)
PY

cat > "$WORK/dosbox.conf" <<EOF
[sdl]
output=surface
[dosbox]
machine=svga_s3
[mixer]
nosound=false
rate=44100
blocksize=1024
[autoexec]
mount c $WORK
imgmount d $WORK/test.cue -t iso
c:
CDTEST.EXE
exit
EOF

echo "== running CDPLAYER's driver layer against a mounted audio disc =="
( cd "$WORK" && \
  SDL_VIDEODRIVER=dummy \
  SDL_AUDIODRIVER=disk \
  SDL_DISKAUDIOFILE="$WORK/audio.raw" \
  timeout 180 dosbox-x -conf "$WORK/dosbox.conf" -nolog -fastlaunch >/dev/null 2>&1 )

[ -f "$WORK/CDTEST.TXT" ] || { echo "ERR CDTEST.EXE produced no report"; exit 1; }
report="$(sed 's/\r$//' "$WORK/CDTEST.TXT")"
printf '%s\n' "$report" | sed 's/^/         /'

fails=0
ck() {
    if [ "$1" = "1" ]; then printf '  [ OK ] %s\n' "$2"
    else printf '  [FAIL] %s\n' "$2"; fails=$((fails+1)); fi
}
has() { printf '%s\n' "$report" | grep -qF "$1" && echo 1 || echo 0; }

echo "== the report =="
ck "$(has 'MSCDEX OK')"                       "MSCDEX installation check answered"
ck "$(has 'TOC low=1 high=3 audio=3 data=0')" "TOC: 3 tracks, all audio, none data"
ck "$(has 'TOTAL frames=1125')"               "total length 1125 frames (15 s at 75 fps)"
# Red Book addresses carry the standard 2-second (150-frame) pregap.
ck "$(has 'TRACK 1 start=150 frames=375')"    "track 1 starts at 150, runs 375 frames"
ck "$(has 'TRACK 2 start=525 frames=375')"    "track 2 starts at 525, runs 375 frames"
ck "$(has 'TRACK 3 start=900 frames=375')"    "track 3 starts at 900, runs 375 frames"
ck "$(has 'PLAY OK track=2')"                 "play accepted for track 2"
ck "$(has 'QCHANNEL secs=')"                  "Q-channel position readable while playing"
ck "$(has 'STOP OK')"                         "stop accepted"
ck "$(has 'DONE')"                            "probe ran to the end"

echo "== the audio that actually came out =="
[ -s "$WORK/audio.raw" ] || { echo "  [FAIL] no audio was captured"; exit 1; }

python3 - "$WORK/audio.raw" <<'PY'
import math, struct, sys
SR   = 44100.0
WANT = 880.0          # the tone written into track 2
raw  = open(sys.argv[1], 'rb').read()
n    = len(raw) // 2
left = struct.unpack('<%dh' % n, raw[:n * 2])[0::2]

W   = 882
env = [math.sqrt(sum(float(x) * x for x in left[i:i+W]) / W)
       for i in range(0, len(left) - W, W)]
loud = [i for i, v in enumerate(env) if v > 500]
if not loud:
    print("  [FAIL] the disc never sounded")
    sys.exit(1)

a   = loud[0] * W + int(0.10 * SR)
seg = left[a:a + 4096]
def goertzel(f):
    k = 2.0 * math.cos(2.0 * math.pi * f / SR)
    s1 = s2 = 0.0
    for x in seg:
        s0 = x + k * s1 - s2
        s2, s1 = s1, s0
    return math.sqrt(abs(s1 * s1 + s2 * s2 - k * s1 * s2))
best_f, best_m, f = 0.0, -1.0, 150.0
while f < 2600.0:
    m = goertzel(f)
    if m > best_m:
        best_f, best_m = f, m
    f += 2.0

secs = len(loud) * W / SR
print("  [ OK ] audio played for %.1f s" % secs)
err = abs(best_f - WANT) / WANT * 100.0
if err <= 2.0:
    print("  [ OK ] the tone is %.0f Hz - track 2's, so the right track played"
          % best_f)
    sys.exit(0)
print("  [FAIL] the tone is %.0f Hz, expected track 2's %.0f Hz "
      "(track 1 is 440, track 3 is 220)" % (best_f, WANT))
sys.exit(1)
PY
audio_rc=$?
fails=$((fails + audio_rc))

if [ "$fails" -eq 0 ]; then
    echo "== CD audio tests PASSED =="
    exit 0
fi
echo "== CD audio tests FAILED ($fails) =="
exit 1
