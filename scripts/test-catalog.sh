#!/usr/bin/env bash
# =====================================================================
#  test-catalog.sh  -  E2E: install a game over the network, under QEMU
# ---------------------------------------------------------------------
#  Boots the CASTALIA DOS floppy in QEMU with an emulated NE2000 card on
#  QEMU's user-mode network, a FAT16 hard disk as C: holding an installed
#  Castalia (CATALOG.EXE + the network kit), and a web server on the host
#  publishing a test catalogue.  Then, from DOS:
#
#    1. CATALOG /LIST           loads the packet driver named in
#                               CASTALIA.INI, gets an address by DHCP,
#                               downloads CATALOG.TXT, lists it
#    2. CATALOG /INSTALL BADCRC a download that does not match its CRC:
#                               must be refused, GAMES.INI untouched
#    3. CATALOG /INSTALL DEMO   download, check, unpack (a program, a
#                               batch file and a subdirectory), register
#    4. the installed game is run from C:\GAMES\DEMO
#    5. HTGET run on its own must fail: CATALOG unloaded the driver it
#       loaded, so the memory is back for games
#
#  and the host reads C: back to check every step.  A 6-byte COM file
#  writes to QEMU's isa-debug-exit port, so the guest ends the run
#  itself instead of waiting out a timeout.
#
#  Usage:   scripts/test-catalog.sh [floppy-image]   (default: dist/*.img)
#  Env:     NET_TIMEOUT=240   seconds before giving up on the guest
#  Needs:   qemu-system-i386, mtools, python3, build/catalog.exe,
#           build/snake.exe, floppy/payload/NET (fetch-payload.sh
#           --with-net).   Exit 0 = passed, 1 = failed, 2 = missing.
# =====================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TIMEOUT="${NET_TIMEOUT:-240}"

err()  { printf 'ERR  %s\n' "$*" >&2; }
info() { printf '  %s\n' "$*"; }
ok()   { printf '  OK   %s\n' "$*"; }

for t in qemu-system-i386 mcopy mmd mformat mpartition python3; do
    command -v "$t" >/dev/null 2>&1 || { err "missing tool: $t"; exit 2; }
done
IMG="${1:-$(find "$ROOT/dist" -maxdepth 1 -name '*.img' -type f 2>/dev/null | sort | head -1)}"
if [ -z "$IMG" ] || [ ! -f "$IMG" ]; then
    err "no floppy image; build one first"
    exit 2
fi
IMG="$(cd "$(dirname "$IMG")" && pwd)/$(basename "$IMG")"
for f in build/catalog.exe build/snake.exe floppy/payload/NET/NE2000.COM \
         floppy/payload/NET/HTGET.EXE floppy/payload/NET/UNZIP.EXE; do
    [ -f "$ROOT/$f" ] || { err "$f missing (wmake; fetch-payload.sh --with-net)"; exit 2; }
done

WORK="$(mktemp -d)"
HTTP_PID=""
trap '[ -n "$HTTP_PID" ] && kill "$HTTP_PID" 2>/dev/null; rm -rf "$WORK"' EXIT
cd "$WORK"

# ---- 1. The web side: two games and a catalogue -------------------------
mkdir -p www/catalog www/games src
python3 - "$ROOT/build/snake.exe" <<'PY'
import sys, zipfile
with zipfile.ZipFile("www/games/DEMO.ZIP", "w", zipfile.ZIP_DEFLATED) as z:
    z.write(sys.argv[1], "SNAKE.EXE")
    z.writestr("PLAYME.BAT", "@ECHO OFF\r\nECHO CASTALIA-GAME-RAN> C:\\RAN.TXT\r\n")
    z.writestr("DATA/README.TXT", "Unpacked into a subdirectory.\r\n")
with zipfile.ZipFile("www/games/BADCRC.ZIP", "w") as z:
    z.writestr("PLAYME.BAT", "@ECHO OFF\r\n")
PY
cat > src/manifest.json <<'JSON'
[
 {"id": "DEMO", "name": "Catalogue Demo", "year": 2026, "genre": "Test",
  "cpu": 386, "exe": "PLAYME.BAT", "profile": "CLEAN", "sound": "NONE",
  "mouse": false, "license": "MIT", "notes": "Installed by the E2E test.",
  "zip": "DEMO.ZIP", "source": "scripts/test-catalog.sh"},
 {"id": "BADCRC", "name": "Damaged Download", "year": 2026, "genre": "Test",
  "cpu": 386, "exe": "PLAYME.BAT", "profile": "CLEAN", "sound": "NONE",
  "mouse": false, "license": "MIT", "notes": "Its CRC is wrong on purpose.",
  "zip": "BADCRC.ZIP", "source": "scripts/test-catalog.sh"}
]
JSON
PORT=$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1])')
python3 "$ROOT/tools/catalog/mkcatalog.py" build src/manifest.json www/games \
    "http://10.0.2.2:$PORT/games" -o www/catalog/CATALOG.TXT >/dev/null
# Corrupt one CRC, and add a line no reader may accept.
python3 - <<'PY'
p = "www/catalog/CATALOG.TXT"
lines = open(p, "rb").read().decode().split("\r\n")
out = []
for l in lines:
    if l.startswith("BADCRC|"):
        f = l.split("|"); f[7] = "00000000"; l = "|".join(f)
    out.append(l)
out.insert(2, "EVIL|Bad|2026|x|386|1|1|00000000|..\\COMMAND.COM|CLEAN|NONE|no|x|http://x/A.ZIP|x")
open(p, "wb").write("\r\n".join(out).encode())
PY
(cd www && exec python3 -m http.server "$PORT" --bind 127.0.0.1 >"$WORK/http.log" 2>&1) &
HTTP_PID=$!
info "catalogue served on port $PORT (10.0.2.2:$PORT from the guest)"

# ---- 2. C: - an installed Castalia --------------------------------------
dd if=/dev/zero of=hd.img bs=512 count=$((64 * 16 * 32)) status=none
printf 'drive c: file="%s/hd.img" partition=1\nmtools_skip_check=1\n' "$WORK" > mtoolsrc
export MTOOLSRC="$WORK/mtoolsrc"
mpartition -I -c -t 64 -h 16 -s 32 c: >/dev/null 2>&1
mformat c: >/dev/null
for d in CASTALIA CASTALIA/BIN CASTALIA/CFG CASTALIA/NET CASTALIA/BACKUP GAMES TEMP; do
    mmd "c:/$d"
done
mcopy -o "$ROOT/build/catalog.exe" c:/CASTALIA/BIN/CATALOG.EXE
for f in "$ROOT"/floppy/payload/NET/*; do
    mcopy -o "$f" "c:/CASTALIA/NET/$(basename "$f")"
done
unix2dos_copy() { sed 's/\r*$/\r/' "$1" > "$WORK/crlf.tmp"; mcopy -o "$WORK/crlf.tmp" "$2"; }
unix2dos_copy "$ROOT/config/GAMES.INI" c:/CASTALIA/CFG/GAMES.INI
# QEMU's ne2k_isa sits at 0x300, IRQ 9 - the same line a user writes for
# their own card.
sed -e "s|^driver *=.*|driver     = NE2000.COM 0x60 9 0x300|" \
    -e "s|^url *=.*|url        = http://10.0.2.2:$PORT/catalog/CATALOG.TXT|" \
    "$ROOT/config/CASTALIA.INI" > castalia.ini
grep -q "10.0.2.2:$PORT" castalia.ini || { err "config/CASTALIA.INI has no [catalog] url line"; exit 1; }
unix2dos_copy castalia.ini c:/CASTALIA/CFG/CASTALIA.INI
mcopy -o c:/CASTALIA/CFG/GAMES.INI games.before

# ---- 3. A: - the Castalia floppy, told what to do -----------------------
cp "$IMG" boot.img
printf '\xb0\x00\xe6\xf4\xcd\x20' > QUIT.COM      # mov al,0 / out 0f4h,al / int 20h
mcopy -o -i boot.img QUIT.COM ::/QUIT.COM
cat > ae.bat <<'BAT'
@ECHO OFF
C:
CD \CASTALIA\BIN
CATALOG /LIST > C:\LIST.TXT
IF ERRORLEVEL 1 ECHO LIST-FAILED> C:\RES.TXT
IF NOT ERRORLEVEL 1 ECHO LIST-OK> C:\RES.TXT
CATALOG /INSTALL BADCRC > C:\BAD.TXT
IF ERRORLEVEL 4 ECHO BADCRC-REFUSED>> C:\RES.TXT
CATALOG /INSTALL DEMO > C:\INST.TXT
IF NOT ERRORLEVEL 1 ECHO INSTALL-OK>> C:\RES.TXT
IF EXIST C:\GAMES\DEMO\PLAYME.BAT CALL C:\GAMES\DEMO\PLAYME.BAT
SET MTCPCFG=C:\CASTALIA\NET\MTCP.CFG
C:\CASTALIA\NET\HTGET.EXE -quiet -o C:\AFTER.TXT http://10.0.2.2/x > C:\AFTER.LOG
IF ERRORLEVEL 1 ECHO DRIVER-UNLOADED>> C:\RES.TXT
ECHO DONE>> C:\RES.TXT
A:\QUIT.COM
BAT
sed 's/$/\r/' ae.bat > ae.crlf
mcopy -o -i boot.img ae.crlf ::/AUTOEXEC.BAT

info "booting in QEMU (NE2000 on user networking; up to ${TIMEOUT}s)"
set +e
timeout "$TIMEOUT" qemu-system-i386 \
    -M isapc -cpu 486 -m 16 \
    -drive file=boot.img,format=raw,if=floppy,index=0 \
    -drive file=hd.img,format=raw,if=ide,index=0 \
    -boot a -display none -no-reboot \
    -netdev user,id=n0 -device ne2k_isa,netdev=n0,iobase=0x300,irq=9 \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 >/dev/null 2>&1
qrc=$?
set -e
[ "$qrc" = "124" ] && info "the guest did not finish in ${TIMEOUT}s - checking what it got to"

# ---- 4. What happened ------------------------------------------------------
get() { mcopy -o "c:/$1" "$2" 2>/dev/null && tr -d '\r' < "$2" > "$2.txt" && return 0; : > "$2.txt"; return 1; }
get RES.TXT res || true
get LIST.TXT list || true
get BAD.TXT bad || true
get INST.TXT inst || true
get CASTALIA/CFG/GAMES.INI games.after || true
get RAN.TXT ran || true

show() { echo "  --- $1 ---"; sed 's/^/    /' "$2.txt" | tail -n "${3:-20}"; }
show "results" res
show "CATALOG /LIST" list 8
show "CATALOG /INSTALL BADCRC" bad 6
show "CATALOG /INSTALL DEMO" inst 10
echo

fails=0
check() { if eval "$2"; then ok "$1"; else printf '  FAIL %s\n' "$1"; fails=$((fails + 1)); fi; }
check "the driver loaded, DHCP answered and the catalogue was listed" "grep -qx LIST-OK res.txt"
check "the list shows both games" "grep -q '^DEMO ' list.txt && grep -q '^BADCRC ' list.txt"
check "the hostile line was skipped, and counted" "grep -q '1 line(s) skipped' list.txt && ! grep -q '^EVIL' list.txt"
check "a download with the wrong CRC was refused" "grep -qx BADCRC-REFUSED res.txt && grep -q 'damaged' bad.txt"
check "the refused game was not registered" "! grep -q '^\[BADCRC\]' games.after.txt"
check "the good game installed" "grep -qx INSTALL-OK res.txt"
check "its files, subdirectory included, are on C:" \
      "mdir c:/GAMES/DEMO/SNAKE.EXE >/dev/null 2>&1 && mdir c:/GAMES/DEMO/DATA/README.TXT >/dev/null 2>&1"
check "the download was cleaned up" "! mdir c:/CASTALIA/NET/DEMO.ZIP >/dev/null 2>&1"
check "it is in GAMES.INI with its path" \
      "grep -q '^\[DEMO\]' games.after.txt && grep -qi 'path *= *C:.GAMES.DEMO' games.after.txt"
check "everything GAMES.INI had before is still there" \
      "tr -d '\r' < games.before | diff -q - <(head -n \$(tr -d '\r' < games.before | wc -l) games.after.txt) >/dev/null"
check "GAMES.INI was backed up first" "mdir c:/CASTALIA/BACKUP/GAMES.INI >/dev/null 2>&1"
check "the installed game runs" "grep -qx CASTALIA-GAME-RAN ran.txt"
check "the packet driver was unloaded afterwards" "grep -qx DRIVER-UNLOADED res.txt"
check "the guest reached the end" "grep -qx DONE res.txt"
check "the guest fetched over HTTP from the host" "grep -q 'GET /games/DEMO.ZIP' http.log"

echo
if [ "$fails" -eq 0 ]; then
    ok "CATALOGUE TEST PASSED - a game came over the network and into the launcher"
    exit 0
fi
err "$fails catalogue check(s) failed"
exit 1
