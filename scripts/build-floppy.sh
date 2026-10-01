#!/usr/bin/env bash
# =====================================================================
#  build-floppy.sh  -  build the CASTALIA DOS bootable 1.44 MB floppy
# ---------------------------------------------------------------------
#  Turns the recipe in scripts/build-floppy.md into a runnable script.
#  Uses only free tools: Open Watcom wmake (to build the .exe tools),
#  mtools (mformat/mmd/mcopy/mdir) to lay down a FAT12 image, and dd.
#
#  Usage:
#     scripts/build-floppy.sh                 full bootable build
#     scripts/build-floppy.sh --manifest      list/validate the layout only
#                                             (coreutils only; no image)
#     scripts/build-floppy.sh --staging       image without a boot sector
#     scripts/build-floppy.sh --no-tools      skip wmake; use build/*.exe
#     scripts/build-floppy.sh -o out.img -v 0.2 -c peniscola
#     scripts/build-floppy.sh --help
#
#  The FreeDOS payload (KERNEL.SYS, COMMAND.COM, memory managers, base
#  utilities) and the FreeDOS floppy boot sector are NOT in this repo for
#  licensing reasons; gather them into floppy/payload/ and floppy/fdboot.bin
#  first (see scripts/build-floppy.md and docs/LICENSE-STRATEGY.md).
#
#  Exit status: 0 success; 1 usage/validation error; 2 missing tool.
# =====================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# ---- defaults -------------------------------------------------------
OUT=""
PAYLOAD="$ROOT/floppy/payload"
BOOTSEC="$ROOT/floppy/fdboot.bin"
VERSION="0.2"
CODENAME="peniscola"
LABEL="CASTALIA"
MODE="build"            # build | manifest | staging
BUILD_TOOLS=1

# ---- colours (only if stdout is a tty) ------------------------------
if [ -t 1 ]; then
    C_OK=$'\033[32m'; C_WARN=$'\033[33m'; C_ERR=$'\033[31m'; C_OFF=$'\033[0m'
else
    C_OK=""; C_WARN=""; C_ERR=""; C_OFF=""
fi
info()  { printf '%s\n' "  $*"; }
ok()    { printf '%s%s%s\n' "$C_OK"   "  OK   $*" "$C_OFF"; }
warn()  { printf '%s%s%s\n' "$C_WARN" "  WARN $*" "$C_OFF"; }
err()   { printf '%s%s%s\n' "$C_ERR"  "  ERR  $*" "$C_OFF" >&2; }

usage() {
    cat <<'EOF'
build-floppy.sh - build the CASTALIA DOS bootable 1.44 MB floppy image

Usage:
  scripts/build-floppy.sh                 full bootable build
  scripts/build-floppy.sh --manifest      validate the layout only (no image)
  scripts/build-floppy.sh --staging       image without a boot sector
  scripts/build-floppy.sh --no-tools      skip wmake; use existing build/*.exe

Options:
  -o, --out FILE       output image path
  -p, --payload DIR    FreeDOS payload dir (default floppy/payload)
  -b, --boot FILE      FreeDOS floppy boot sector (default floppy/fdboot.bin)
  -v, --version STR    version for the artifact name (default 0.2)
  -c, --codename STR   codename for the artifact name (default peniscola)
      --label STR      FAT volume label (default CASTALIA)
      --manifest       list/validate the planned layout (coreutils only)
      --staging        build a non-bootable staging image
      --no-tools       do not run wmake; use build/*.exe as-is
  -h, --help           this help

Needs: Open Watcom wmake (tools), mtools + dd (image). The FreeDOS payload
and boot sector are gathered into floppy/ first; see scripts/build-floppy.md.
EOF
    exit "${1:-0}"
}

# ---- argument parsing ----------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        -o|--out)      OUT="$2"; shift 2 ;;
        -p|--payload)  PAYLOAD="$2"; shift 2 ;;
        -b|--boot)     BOOTSEC="$2"; shift 2 ;;
        -v|--version)  VERSION="$2"; shift 2 ;;
        -c|--codename) CODENAME="$2"; shift 2 ;;
        --label)       LABEL="$2"; shift 2 ;;
        --manifest)    MODE="manifest"; shift ;;
        --staging)     MODE="staging"; shift ;;
        --no-tools)    BUILD_TOOLS=0; shift ;;
        -h|--help)     usage 0 ;;
        *) err "unknown option: $1"; usage 1 ;;
    esac
done

[ -n "$OUT" ] || OUT="$ROOT/dist/castalia-dos-${VERSION}-${CODENAME}-boot.img"

# ---- the copy plan: "src|dosdir|required" ---------------------------
# dosdir is the destination directory on the image (mtools ::/... form).
PLAN=()
add() { PLAN+=("$1|$2|$3"); }

# Root: kernel, shell, and the FLOPPY-specific boot files (A: paths;
# the installed-system templates ride in ::/INSTALL/ for SETUP).
add "$PAYLOAD/KERNEL.SYS"        "::/"              1
add "$PAYLOAD/COMMAND.COM"       "::/"              1
add "$ROOT/config/floppy/CONFIG.SYS"   "::/"        1
add "$ROOT/config/floppy/AUTOEXEC.BAT" "::/"        1

# Installed-system templates (written to C:\ by SETUP.EXE).
add "$ROOT/config/CONFIG.SYS"    "::/INSTALL/"      1
add "$ROOT/config/AUTOEXEC.BAT"  "::/INSTALL/"      1

# DOS core + memory managers (from the FreeDOS payload).
add "$PAYLOAD/HIMEMX.EXE"        "::/DOS/"          1
add "$PAYLOAD/JEMM386.EXE"       "::/DOS/"          1
add "$PAYLOAD/MEM.EXE"           "::/DOS/"          1
add "$PAYLOAD/FDISK.EXE"         "::/DOS/"          1
add "$PAYLOAD/FORMAT.EXE"        "::/DOS/"          1
add "$PAYLOAD/SYS.COM"           "::/DOS/"          1
add "$PAYLOAD/SHSUCDX.COM"       "::/DOS/"          0
add "$PAYLOAD/XCOPY.EXE"         "::/DOS/"          0
add "$PAYLOAD/CHKDSK.EXE"        "::/DOS/"          0

# Castalia-bundled drivers (CONFIG.SYS and AUTOEXEC.BAT reference these
# under C:\CASTALIA\DRV after install; SETUP copies A:\CASTALIA -> C:).
add "$PAYLOAD/UIDE.SYS"          "::/CASTALIA/DRV/" 0
add "$PAYLOAD/CTMOUSE.EXE"       "::/CASTALIA/DRV/" 0

# Castalia tools (built into build/).  Lowercase source names become
# uppercase 8.3 names on the FAT image.
add "$ROOT/build/castalia.exe"   "::/CASTALIA/BIN/" 1
add "$ROOT/build/launch.exe"     "::/CASTALIA/BIN/" 1
add "$ROOT/build/hwinfo.exe"     "::/CASTALIA/BIN/" 0
add "$ROOT/build/setsound.exe"   "::/CASTALIA/BIN/" 0
add "$ROOT/build/memprof.exe"    "::/CASTALIA/BIN/" 0
add "$ROOT/build/safeboot.exe"   "::/CASTALIA/BIN/" 0
add "$ROOT/build/cfgedit.exe"    "::/CASTALIA/BIN/" 0
add "$ROOT/build/gamecfg.exe"    "::/CASTALIA/BIN/" 0
add "$ROOT/build/setup.exe"      "::/CASTALIA/BIN/" 0
add "$ROOT/build/castfm.exe"     "::/CASTALIA/BIN/" 0
add "$ROOT/build/castmark.exe"   "::/CASTALIA/BIN/" 0
add "$ROOT/build/castcopy.exe"   "::/CASTALIA/BIN/" 0
add "$ROOT/build/castdoc.exe"    "::/CASTALIA/BIN/" 0
add "$ROOT/build/banner.exe"     "::/CASTALIA/BIN/" 0
add "$ROOT/build/snake.exe"      "::/CASTALIA/BIN/" 0
add "$ROOT/build/puzzle.exe"     "::/CASTALIA/BIN/" 0
add "$ROOT/build/almena.exe"     "::/CASTALIA/BIN/" 0
add "$ROOT/build/minas.exe"      "::/CASTALIA/BIN/" 0
add "$ROOT/build/siege.exe"      "::/CASTALIA/BIN/" 0
add "$ROOT/build/reversi.exe"    "::/CASTALIA/BIN/" 0
add "$ROOT/build/barrels.exe"    "::/CASTALIA/BIN/" 0
add "$ROOT/build/solitare.exe"   "::/CASTALIA/BIN/" 0
add "$ROOT/build/cdplayer.exe"   "::/CASTALIA/BIN/" 0
add "$ROOT/build/saver.exe"      "::/CASTALIA/BIN/" 0
add "$ROOT/build/casttour.exe"   "::/CASTALIA/BIN/" 0
add "$ROOT/build/castlink.exe"   "::/CASTALIA/BIN/" 0
add "$ROOT/build/undel.exe"      "::/CASTALIA/BIN/" 0
add "$ROOT/build/help.exe"       "::/CASTALIA/BIN/" 0
add "$ROOT/build/castedit.exe"   "::/CASTALIA/BIN/" 0
add "$ROOT/build/castid.exe"     "::/CASTALIA/BIN/" 0
add "$ROOT/config/GAMES.BAT"     "::/CASTALIA/BIN/" 1

# Castalia config.
# Licence notice on the media: the release gate in
# docs/LICENSE-STRATEGY.md requires it, and GPL components are aboard.
add "$ROOT/LICENSES/THIRD-PARTY.txt" "::/CASTALIA/LICENSE/" 1
add "$ROOT/LICENSES/GPLv2.txt"       "::/CASTALIA/LICENSE/" 0
# HIMEMX and JEMM386 are partly under the Artistic License, which also
# asks for its text to travel with the binaries.  8.3 name on purpose.
add "$ROOT/LICENSES/Artistic-1.0.txt" "::/CASTALIA/LICENSE/ARTISTIC.TXT" 1

add "$ROOT/config/CASTALIA.INI"  "::/CASTALIA/CFG/" 1
add "$ROOT/config/PROFILES.INI"  "::/CASTALIA/CFG/" 1
add "$ROOT/config/GAMES.INI"     "::/CASTALIA/CFG/" 1
add "$ROOT/config/SOUND.BAT"     "::/CASTALIA/CFG/" 1

# Help pages.
for f in "$ROOT"/help/*.TXT "$ROOT/help/HELP.IDX"; do
    add "$f" "::/CASTALIA/HELP/" 0
done

# Directory tree to create on the image.
DIRS=( "::/DOS" "::/INSTALL" "::/CASTALIA" "::/CASTALIA/BIN"
       "::/CASTALIA/DRV" "::/CASTALIA/CFG" "::/CASTALIA/HELP"
       "::/CASTALIA/LICENSE" )

dospath() { # turn "::/CASTALIA/BIN/" + basename into a DOS-ish path
    local d="${1#::}"
    case "$d" in
        */) printf '%s%s' "$d" "$2" ;;
        *)  printf '%s' "$d" ;;          # destination names the file itself
    esac
}

# Castalia's own text files (configs, batch files, help) go onto the disk
# with CRLF line ends whatever the checkout used: a Linux checkout has LF,
# a Windows one CRLF.  Vendored licence texts are copied byte for byte
# (see .gitattributes), so they are not in this list.
is_text() {
    case "$1" in
        "$ROOT"/config/*|"$ROOT"/help/*) return 0 ;;
    esac
    return 1
}

# ---- manifest mode (coreutils only; validate the layout) ------------
run_manifest() {
    local miss_req=0 miss_opt=0 present=0 entry src dst req base
    printf '\nCASTALIA DOS floppy build - layout manifest\n'
    printf 'Payload : %s\n' "$PAYLOAD"
    printf 'Boot    : %s\n' "$BOOTSEC"
    printf 'Output  : %s\n\n' "$OUT"
    printf '  %-6s  %-26s  %s\n' "STATUS" "IMAGE PATH" "SOURCE"
    printf '  %-6s  %-26s  %s\n' "------" "--------------------------" "------"
    for entry in "${PLAN[@]}"; do
        IFS='|' read -r src dst req <<<"$entry"
        base="$(basename "$src")"
        if [ -f "$src" ]; then
            ok "$(printf '%-26s  %s' "$(dospath "$dst" "$base")" "$src")"
            present=$((present+1))
        elif [ "$req" = "1" ]; then
            err "$(printf '%-26s  %s (REQUIRED, missing)' "$(dospath "$dst" "$base")" "$src")"
            miss_req=$((miss_req+1))
        else
            warn "$(printf '%-26s  %s (optional, missing)' "$(dospath "$dst" "$base")" "$src")"
            miss_opt=$((miss_opt+1))
        fi
    done
    printf '\n  boot sector: '
    if [ -f "$BOOTSEC" ]; then printf '%sfound%s\n' "$C_OK" "$C_OFF"
    else printf '%smissing (image would not be bootable)%s\n' "$C_WARN" "$C_OFF"; fi
    printf '\nSummary: %d present, %d required missing, %d optional missing.\n' \
        "$present" "$miss_req" "$miss_opt"
    if [ "$miss_req" -gt 0 ]; then
        printf '%sManifest incomplete: gather the required files first.%s\n' \
            "$C_ERR" "$C_OFF"
        return 1
    fi
    printf '%sManifest complete: ready for a full build.%s\n' "$C_OK" "$C_OFF"
    return 0
}

# ---- tool checks ----------------------------------------------------
need() { command -v "$1" >/dev/null 2>&1 || { err "required tool '$1' not found"; return 1; }; }

check_image_tools() {
    local missing=0
    for t in dd mformat mmd mcopy mdir; do
        command -v "$t" >/dev/null 2>&1 || { err "missing tool: $t"; missing=1; }
    done
    if [ "$missing" -ne 0 ]; then
        err "install mtools and coreutils, e.g.:"
        err "   Debian/Ubuntu:  sudo apt-get install mtools"
        err "   Fedora:         sudo dnf install mtools"
        err "   macOS:          brew install mtools"
        exit 2
    fi
}

# ---- build the Castalia tools ---------------------------------------
build_tools() {
    if [ "$BUILD_TOOLS" -eq 0 ]; then
        info "skipping tool build (--no-tools); using existing build/*.exe"
        return
    fi
    if ! command -v wmake >/dev/null 2>&1; then
        err "wmake (Open Watcom) not found; cannot build the tools."
        err "install Open Watcom V2 and source owsetenv.sh, or pass --no-tools"
        err "to build the image from pre-built build/*.exe."
        exit 2
    fi
    info "building the Castalia tools with wmake..."
    ( cd "$ROOT" && wmake )
    ok "tools built into build/"
}

# ---- assemble the image ---------------------------------------------
assemble() {
    local boot_ok=1
    check_image_tools

    # Validate required inputs.
    local entry src dst req base missing=0
    for entry in "${PLAN[@]}"; do
        IFS='|' read -r src dst req <<<"$entry"
        if [ "$req" = "1" ] && [ ! -f "$src" ]; then
            err "required file missing: $src"; missing=1
        fi
    done
    if [ "$missing" -ne 0 ]; then
        err "gather the missing files (see scripts/build-floppy.md) and retry."
        exit 1
    fi

    if [ ! -f "$BOOTSEC" ]; then
        if [ "$MODE" = "staging" ]; then
            warn "no boot sector; building a NON-BOOTABLE staging image."
            boot_ok=0
        else
            err "boot sector not found: $BOOTSEC"
            err "provide the FreeDOS floppy boot sector, or use --staging."
            exit 1
        fi
    fi

    mkdir -p "$(dirname "$OUT")"
    info "creating blank 1.44 MB image: $OUT"
    dd if=/dev/zero of="$OUT" bs=512 count=2880 status=none

    if [ "$boot_ok" -eq 1 ]; then
        info "formatting FAT12 with boot sector"
        mformat -i "$OUT" -f 1440 -v "$LABEL" -B "$BOOTSEC" ::
    else
        info "formatting FAT12 (no boot sector)"
        mformat -i "$OUT" -f 1440 -v "$LABEL" ::
    fi

    info "creating directory tree"
    for d in "${DIRS[@]}"; do mmd -i "$OUT" "$d"; done

    info "copying files"
    local crlf
    crlf="$(mktemp)"
    for entry in "${PLAN[@]}"; do
        IFS='|' read -r src dst req <<<"$entry"
        [ -f "$src" ] || continue          # optional missing already warned
        case "$dst" in */) dst="$dst$(basename "$src")" ;; esac
        if is_text "$src"; then
            sed 's/\r$//; s/$/\r/' "$src" > "$crlf"
            mcopy -i "$OUT" -o "$crlf" "$dst"
        else
            mcopy -i "$OUT" -o "$src" "$dst"
        fi
    done
    rm -f "$crlf"

    info "verifying"
    mdir -i "$OUT" ::/ >/dev/null
    mdir -i "$OUT" ::/CASTALIA/BIN >/dev/null
    # The disk is nearly full, and a tool that grows past the edge only
    # shows up as an mcopy "disk full".  Say how close it is every build.
    info "free on the image: $(mdir -i "$OUT" ::/ | sed -n 's/^ *\([0-9][0-9 ]*\) bytes free.*/\1/p') bytes"

    if command -v sha256sum >/dev/null 2>&1; then
        ( cd "$(dirname "$OUT")" && sha256sum "$(basename "$OUT")" \
            > "$(basename "$OUT").sha256" )
        ok "checksum: $(basename "$OUT").sha256"
    fi

    ok "image built: $OUT"
    if [ "$boot_ok" -eq 1 ]; then
        ok "bootable: yes"
    else
        warn "bootable: no (staging)"
    fi
    info "test it:  dosbox-x $OUT     (then 86Box as a 386SX; see docs/TESTING.md)"
}

# ---- corresponding source, shipped beside the image ------------------
#  The GPL components' source does not fit on a 1.44 MB disk, so it goes
#  into a companion archive published next to the image.  GPLv2 section 3
#  accepts that: equivalent access to the source from the same place.
#  THIRD-PARTY.txt on the disk points here.
pack_sources() {
    local zip="${OUT%.img}"
    zip="${zip%-boot}-sources.zip"
    if [ ! -d "$PAYLOAD/SOURCES" ]; then
        warn "no payload/SOURCES: this image is NOT releasable."
        warn "run scripts/fetch-payload.sh --with-sources, then rebuild."
        return 0
    fi
    if ! command -v python3 >/dev/null 2>&1; then
        warn "python3 not found; source archive not written (image is NOT releasable)"
        return 0
    fi
    python3 - "$zip" "$PAYLOAD/SOURCES" "$ROOT" <<'PYZIP'
import os, sys, zipfile
out, srcdir, root = sys.argv[1:4]
readme = (
    "CASTALIA DOS - corresponding source for the third-party components\r\n"
    "\r\n"
    "SOURCES/*-SRC.ZIP   source archive of each FreeDOS component on the disk\r\n"
    "SOURCES/KERNEL-SRC.ZIP  FreeDOS kernel 2043 source; the Castalia kernel is\r\n"
    "                    this source with CASTALIA/patch-kernel-src.py applied\r\n"
    "                    and built by CASTALIA/build-kernel.sh\r\n"
    "CASTALIA/rebrand-dos.py  the in-place edit made to FreeCOM's COMMAND.COM\r\n"
    "LICENSES/           licence texts and the component manifest\r\n"
)
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("README.TXT", readme)
    for name in sorted(os.listdir(srcdir)):
        z.write(os.path.join(srcdir, name), "SOURCES/" + name)
    for name in ("patch-kernel-src.py", "build-kernel.sh", "rebrand-dos.py"):
        z.write(os.path.join(root, "scripts", name), "CASTALIA/" + name)
    lic = os.path.join(root, "LICENSES")
    for name in sorted(os.listdir(lic)):
        z.write(os.path.join(lic, name), "LICENSES/" + name)
PYZIP
    ok "source archive: $zip"
}

# ---- main -----------------------------------------------------------
case "$MODE" in
    manifest) run_manifest ;;
    build|staging) build_tools; assemble; pack_sources ;;
esac
