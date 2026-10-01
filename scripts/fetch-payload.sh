#!/usr/bin/env bash
# =====================================================================
#  fetch-payload.sh  -  gather the FreeDOS payload for the boot floppy
# ---------------------------------------------------------------------
#  Downloads the FreeDOS 1.3 components that CASTALIA DOS builds on and
#  lays them out where scripts/build-floppy.sh expects them:
#
#     floppy/payload/   KERNEL.SYS COMMAND.COM HIMEMX.EXE JEMM386.EXE
#                       MEM.EXE FDISK.EXE FORMAT.EXE SYS.COM XCOPY.EXE
#                       SHSUCDX.COM CHKDSK.EXE
#     floppy/fdboot.bin the FreeDOS 1.44M FAT12 boot sector
#
#  Sources (official FreeDOS mirrors on ibiblio):
#    - FD13-FloppyEdition.zip : boot disk image -> kernel, shell, live
#      utilities (FDISK/FORMAT/SYS/MEM/XCOPY) and the boot sector
#    - repositories/1.3/base  : himemx.zip jemm.zip shsucdx.zip chkdsk.zip
#
#  LICENSE NOTE: everything fetched here is GPL/free FreeDOS software.
#  These binaries are NOT committed to the repo; CI fetches them per run
#  (cacheable).  Any *public release* built from them must ship sources
#  per docs/LICENSE-STRATEGY.md - CI test artifacts are not releases.
#
#  Usage:
#     scripts/fetch-payload.sh [--cache DIR] [--with-archiver] [--with-sources]
#  --cache DIR keeps the downloaded archives outside the repo so CI can
#  restore them (default: .payload-cache, gitignored).
#  --with-archiver additionally stages the Info-ZIP tools into
#  payload/ARCHIVER/ for the hard-disk distribution; they are left out by
#  default because they do not fit the 1.44 MB rescue floppy.
#  --with-sources stages each package's own SOURCES.ZIP into payload/SOURCES/,
#  which is how the GPL/Artistic source obligation is met on release media.
#
#  Provenance: every archive downloaded (or restored from cache) is logged
#  to payload/PROVENANCE.txt as "SHA256  file  size  URL".
#
#  Needs: curl, unzip, mtools (mcopy), dd.  Exit 0 on success.
# =====================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CACHE="$ROOT/.payload-cache"
#  The Info-ZIP archive tools are opt-in (--with-archiver).  They are NOT
#  on the 1.44 MB rescue floppy: UNZIP.EXE alone is 193 KB and the floppy
#  already carries the kernel, the shell, the DOS utilities, the whole
#  Castalia suite and the help pages.  They belong to the hard-disk
#  distribution, and land in payload/ARCHIVER/ so the floppy plan cannot
#  pick them up by accident.
WITH_ARCHIVER=0
#  --with-sources stages each vendored package's own SOURCE/<NAME>/SOURCES.ZIP.
#  That is how the GPL/Artistic source obligation is actually met: CTMOUSE is
#  GPL and HIMEMX is GPL and/or Artistic (both verified from the packages, see
#  LICENSES/MANIFEST.md), and every FreeDOS package helpfully carries its own
#  source, so a release build only has to keep it.
WITH_SOURCES=0

while [ $# -gt 0 ]; do
    case "$1" in
        --cache)          CACHE="$2"; shift 2 ;;
        --with-archiver)  WITH_ARCHIVER=1; shift ;;
        --with-sources)   WITH_SOURCES=1; shift ;;
        *) printf 'ERR  unknown option: %s\n' "$1" >&2; exit 1 ;;
    esac
done

PAYLOAD="$ROOT/floppy/payload"
BOOTSEC="$ROOT/floppy/fdboot.bin"

FD_URL="https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/distributions/1.3/official/FD13-FloppyEdition.zip"
PKG_ROOT="https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/1.3"

err()  { printf 'ERR  %s\n' "$*" >&2; }
info() { printf '  %s\n' "$*"; }
warn() { printf 'WARN %s\n' "$*" >&2; }
ok()   { printf '  OK   %s\n' "$*"; }

for t in curl unzip mcopy dd python3; do
    command -v "$t" >/dev/null 2>&1 || { err "missing tool: $t"; exit 2; }
done

mkdir -p "$CACHE" "$PAYLOAD"

#  Download with mirror fallback and a diagnosis worth reading.
#
#  The upstream FreeDOS files live on ibiblio, which has had directory-level
#  outages: in one, /files/distributions/ answered 403 ("Server unable to
#  read htaccess file, denying access to be safe") while /files/repositories/
#  on the same host kept serving 200.  A bare `curl --fail` turns that into
#  "exit code 22" with no hint, and the failure looks like ours when it is
#  not.  So: try each host in turn, and if they all refuse, say plainly what
#  happened, with the HTTP codes, before exiting.
#
#  Every candidate is HTTPS.  An http:// fallback was tempting - it would
#  survive a TLS-only outage - but these archives are a kernel and the
#  executables that go straight into a bootable image.  The pinned digests
#  below would catch a swapped file, but there is no reason to invite one.
#  A failed build is the better outcome.
#
#  HTTPS authenticates the mirror, not the file.  Every archive is also
#  checked against the digest pinned in scripts/payload.sha256, on download
#  AND on a cache hit, so neither a changed upstream file nor a stale or
#  tampered CI cache can reach the image unnoticed.
PINS="$ROOT/scripts/payload.sha256"
pin_check() { # cache file name -> 0 when it matches its pinned digest
    local name="$1" want got
    want=$(awk -v n="$name" '{ sub(/\r$/, "") } $1 !~ /^#/ && $2 == n { print $1 }' "$PINS")
    if [ -z "$want" ]; then
        err "$name has no pinned digest in scripts/payload.sha256"
        return 1
    fi
    got=$(sha256sum "$CACHE/$name" | cut -d' ' -f1)
    [ "$got" = "$want" ] && return 0
    err "$name does not match its pinned digest"
    err "  expected $want"
    err "  got      $got"
    return 1
}

fetch() { # url -> cache file (skip when cached)
    local url="$1" out="$CACHE/$2" u code codes=""
    if [ -s "$out" ]; then
        if pin_check "$2"; then
            info "cached: $2"
            record "$url" "$2"      # a cache hit is still provenance
            return 0
        fi
        warn "discarding the cached $2 and downloading it again"
        rm -f "$out"
    fi
    info "downloading $2 ..."
    for u in "$url" \
             "${url/https:\/\/www.ibiblio.org/https:\/\/ibiblio.org}"; do
        code=$(curl -sL --max-time 600 -o "$out" -w '%{http_code}' "$u" || echo 000)
        if [ "$code" = "200" ] && [ -s "$out" ]; then
            if pin_check "$2"; then
                record "$u" "$2"
                return 0
            fi
            code="digest-mismatch"
        fi
        rm -f "$out"
        codes="$codes  $code $u"
    done
    if [ "${FETCH_SOFT:-0}" = "1" ]; then
        return 1                       # caller wants to try another route
    fi
    err "could not download $2 - every mirror refused:"
    printf '%s\n' "$codes" >&2
    err "This is an upstream mirror problem, not a fault in this repository."
    err "Re-run once the mirror recovers, or prime .payload-cache/$2 by hand"
    err "(it is checked against scripts/payload.sha256 either way)."
    exit 1
}

#  Provenance.  docs/ROADMAP.md Phase 0 asks that every vendored component
#  trace to an upstream URL and a checksum; recording it here means the
#  record is of what was actually fetched, not of what someone believed
#  was fetched.  LICENSES/MANIFEST.md explains how to read this file.
PROV="$PAYLOAD/PROVENANCE.txt"
: > "$PROV"
record() {  # url cachefile -> one provenance line
    local url="$1" f="$CACHE/$2"
    [ -s "$f" ] || return 0
    printf '%s  %s  %s bytes  %s\n' \
        "$(sha256sum "$f" | cut -d' ' -f1)" "$2" "$(stat -c %s "$f")" "$url" \
        >> "$PROV"
}

# ---- 1. The DOS core: FloppyEdition if we can get it, else packages --
#
#  Preferred route: the official FloppyEdition image, which carries the
#  kernel, the shell, the live utilities AND the FAT12 boot sector.
#
#  Fallback route: ibiblio has had directory-level outages where
#  /files/distributions/ answers 403 while /files/repositories/ on the
#  same host keeps serving 200.  Everything the image gave us also exists
#  as packages - except the boot sector, which we assemble from the
#  kernel's own boot/boot.asm with nasm (the same pinned source
#  build-kernel.sh already downloads).  Building the sector from source
#  is if anything cleaner than lifting it out of a shipped binary.
KSRC_URL="https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/dos/kernel/2043/ke2043s.zip"

core_from_packages() {
    info "assembling the DOS core from the package repository"
    extract_pkg base/kernel.zip   BIN/KERNL86.SYS KERNEL.SYS
    extract_pkg base/kernel.zip   BIN/SYS.COM     SYS.COM
    extract_pkg base/freecom.zip  BIN/COMMAND.COM COMMAND.COM
    extract_pkg base/fdisk.zip    BIN/FDISK.EXE   FDISK.EXE
    extract_pkg base/format.zip   BIN/FORMAT.EXE  FORMAT.EXE
    extract_pkg base/mem.zip      BIN/MEM.EXE     MEM.EXE
    extract_pkg base/xcopy.zip    BIN/XCOPY.EXE   XCOPY.EXE

    command -v nasm >/dev/null 2>&1 || {
        err "nasm is needed to assemble the boot sector on this route"
        exit 2
    }
    fetch "$KSRC_URL" "ke2043s.zip"
    info "assembling the FAT12 boot sector from the kernel source"
    rm -rf "$CACHE/bootsrc"
    unzip -o -q "$CACHE/ke2043s.zip" "SOURCE/ke2043/boot/*" -d "$CACHE/bootsrc"
    ( cd "$CACHE/bootsrc/SOURCE/ke2043/boot" &&
      nasm -dISFAT12 boot.asm -o "$BOOTSEC" )
    [ "$(stat -c %s "$BOOTSEC" 2>/dev/null)" = "512" ] || {
        err "the assembled boot sector is not 512 bytes"; exit 1; }
    if ! python3 - "$BOOTSEC" <<'PYBOOT'
import sys
d = open(sys.argv[1], 'rb').read()
if d[510:512] != b'\x55\xaa':
    sys.stderr.write("  boot sector lacks the 55AA signature\n")
    raise SystemExit(1)
raise SystemExit(0)
PYBOOT
    then
        err "the assembled boot sector is not bootable"
        exit 1
    fi
    ok "boot sector assembled from source (512 bytes, 55AA)"
}

core_from_image() {
    info "extracting the 1.44M boot disk image"
    unzip -o -q "$CACHE/FD13-FloppyEdition.zip" "144m/x86BOOT.img" -d "$CACHE"
    BOOTIMG="$CACHE/144m/x86BOOT.img"

    info "extracting kernel, shell, and live utilities from the boot disk"
    mcopy -n -i "$BOOTIMG" ::/KERNEL.SYS            "$PAYLOAD/KERNEL.SYS"
    mcopy -n -i "$BOOTIMG" ::/FREEDOS/BIN/COMMAND.COM "$PAYLOAD/COMMAND.COM"
    mcopy -n -i "$BOOTIMG" ::/FREEDOS/BIN/FDISK.EXE   "$PAYLOAD/FDISK.EXE"
    mcopy -n -i "$BOOTIMG" ::/FREEDOS/BIN/FORMAT.EXE  "$PAYLOAD/FORMAT.EXE"
    mcopy -n -i "$BOOTIMG" ::/FREEDOS/BIN/SYS.COM     "$PAYLOAD/SYS.COM"
    mcopy -n -i "$BOOTIMG" ::/FREEDOS/BIN/MEM.EXE     "$PAYLOAD/MEM.EXE"
    mcopy -n -i "$BOOTIMG" ::/FREEDOS/BIN/XCOPY.EXE   "$PAYLOAD/XCOPY.EXE"

    info "extracting the FAT12 boot sector -> floppy/fdboot.bin"
    dd if="$BOOTIMG" of="$BOOTSEC" bs=512 count=1 status=none
}

# ---- 2. Packages: memory managers, CD stack, mouse, chkdsk ----------
extract_pkg() { # group/pkg.zip member dest
    local rel="$1" member="$2" dest="$3"
    local pkg
    pkg="$(basename "$rel")"
    fetch "$PKG_ROOT/$rel" "$pkg"
    unzip -o -q -j "$CACHE/$pkg" "$member" -d "$PAYLOAD"
    [ -f "$PAYLOAD/$(basename "$member")" ] || { err "missing $member in $pkg"; exit 1; }
    [ "$(basename "$member")" = "$dest" ] || \
        mv "$PAYLOAD/$(basename "$member")" "$PAYLOAD/$dest"
}
#  extract_pkg is defined above; now pick the route for the DOS core.
if FETCH_SOFT=1 fetch "$FD_URL" "FD13-FloppyEdition.zip"; then
    core_from_image
else
    warn "the FloppyEdition is unavailable from every mirror"
    warn "(this is an upstream outage - falling back to the packages)"
    core_from_packages
fi

extract_pkg base/himemx.zip     BIN/HIMEMX.EXE  HIMEMX.EXE
extract_pkg base/jemm.zip       BIN/JEMM386.EXE JEMM386.EXE
extract_pkg base/shsucdx.zip    BIN/SHSUCDX.COM SHSUCDX.COM
extract_pkg base/chkdsk.zip     BIN/CHKDSK.EXE  CHKDSK.EXE
extract_pkg base/ctmouse.zip    BIN/CTMOUSE.EXE CTMOUSE.EXE
extract_pkg drivers/uide.zip    BIN/UIDE.SYS    UIDE.SYS

# ---- 2.4 Archive tools (opt-in) -------------------------------------
#
#  Info-ZIP is the license-clean answer to PKZIP/ARJ/LHA that ROADMAP
#  Addendum A lists.  Two things are worth knowing before trusting them
#  on the flagship 386SX:
#
#  * FreeDOS's UNZIP.EXE is a DJGPP build - it carries the go32 stub and
#    needs a DPMI host.  There is no 16-bit UnZip in the FreeDOS
#    repository (1.2 ships the identical binary), so CWSDPMI comes along
#    with it; a DJGPP program loads CWSDPMI.EXE from the PATH by itself.
#  * ZIP ships both ways.  We take ZIP16.EXE, the real-mode build, so at
#    least creating archives needs no DPMI host at all.
#
#  Licenses: Info-ZIP is permissive (LICENSES/InfoZIP.txt).  CWSDPMI is
#  GPLv2 but its author grants explicit permission to ship the official
#  unmodified binary alongside precise directions to the source, in place
#  of GPLv2 section 3 - which is what MANIFEST.md records.
if [ "$WITH_ARCHIVER" = "1" ]; then
    info "gathering the Info-ZIP archive tools (opt-in)"
    mkdir -p "$PAYLOAD/ARCHIVER"
    ARCH_OUT="$PAYLOAD/ARCHIVER"

    arch_get() {  # group/pkg.zip member dest
        local rel="$1" member="$2" dest="$3" pkg
        pkg="$(basename "$rel")"
        fetch "$PKG_ROOT/$rel" "$pkg"
        unzip -o -q -j "$CACHE/$pkg" "$member" -d "$ARCH_OUT"
        [ -f "$ARCH_OUT/$(basename "$member")" ] || {
            err "missing $member in $pkg"; exit 1; }
        [ "$(basename "$member")" = "$dest" ] || \
            mv "$ARCH_OUT/$(basename "$member")" "$ARCH_OUT/$dest"
    }

    arch_get archiver/unzip.zip   BIN/UNZIP.EXE        UNZIP.EXE
    arch_get archiver/unzip.zip   DOC/UNZIP/LICENSE    UNZIP.LIC
    arch_get archiver/zip.zip     BIN/ZIP16.EXE        ZIP.EXE
    arch_get util/cwsdpmi.zip     BIN/CWSDPMI.EXE      CWSDPMI.EXE
    arch_get util/cwsdpmi.zip     DOC/CWSDPMI/COPYING.CWS CWSDPMI.LIC

    # The 386SX promise is worth a check, not a hope: ZIP must be free of
    # the DJGPP stub, and the DPMI host must actually be there for UNZIP.
    if grep -qa "go32stub" "$ARCH_OUT/ZIP.EXE" 2>/dev/null; then
        err "ZIP.EXE is a DJGPP build - the 16-bit ZIP16.EXE was expected"
        exit 1
    fi
    [ -s "$ARCH_OUT/CWSDPMI.EXE" ] || {
        err "UNZIP.EXE needs a DPMI host and CWSDPMI.EXE is missing"; exit 1; }
    ok "archive tools staged in payload/ARCHIVER (not on the rescue floppy)"
fi

# ---- 2.6 Vendored sources (opt-in) ----------------------------------
#
#  GPLv2 section 3 wants the source of every GPL binary on the media.  Each
#  FreeDOS package ships its own under SOURCE/<NAME>/SOURCES.ZIP, so meeting
#  the obligation is a copy, not a hunt.  Named per component so an auditor
#  can match them to the rows in LICENSES/MANIFEST.md at a glance.
if [ "$WITH_SOURCES" = "1" ]; then
    info "staging vendored sources (GPL/Artistic obligation)"
    mkdir -p "$PAYLOAD/SOURCES"
    #  Every component on the media, not only the ones fetched as packages:
    #  on the FloppyEdition route the shell and the base utilities come out
    #  of the boot image, so their packages are fetched here just for the
    #  source.  The 1.3 packages are the same release as the image.
    for rel in base/kernel base/freecom base/fdisk base/format base/mem \
               base/xcopy base/chkdsk base/himemx base/jemm base/shsucdx \
               base/ctmouse drivers/uide; do
        comp="$(basename "$rel")"
        pkg="$comp.zip"
        fetch "$PKG_ROOT/$rel.zip" "$pkg"
        member=$(unzip -Z1 "$CACHE/$pkg" 2>/dev/null |
                 grep -iE '^SOURCE/.*SOURCES\.ZIP$' | head -1)
        if [ -z "$member" ]; then
            err "$pkg ships no SOURCES.ZIP - its source obligation is unmet"
            exit 1
        fi
        # The kernel package is here for SYS.COM; KERNEL-SRC.ZIP is reserved
        # for the source the Castalia kernel is actually built from.
        name="$(echo "$comp" | tr '[:lower:]' '[:upper:]')"
        [ "$comp" = "kernel" ] && name="SYS"
        unzip -o -q -j "$CACHE/$pkg" "$member" -d "$PAYLOAD/SOURCES"
        mv "$PAYLOAD/SOURCES/$(basename "$member")" \
           "$PAYLOAD/SOURCES/$name-SRC.ZIP"
    done
    # The kernel is the most prominent GPL component of all, and its source
    # is the archive build-kernel.sh rebuilds from rather than something
    # inside a package.  Fetch it if this run has not already: a release
    # that stages every driver's source but not the kernel's would be the
    # one omission nobody could defend.
    [ -s "$CACHE/ke2043s.zip" ] || fetch "$KSRC_URL" "ke2043s.zip"
    cp "$CACHE/ke2043s.zip" "$PAYLOAD/SOURCES/KERNEL-SRC.ZIP"
    n=$(find "$PAYLOAD/SOURCES" -maxdepth 1 -type f | wc -l)
    ok "$n source archive(s) staged in payload/SOURCES"
fi

# ---- 2.5 Rebrand: blank the FreeCOM sign-on banner -------------------
#  Blanks FreeCOM's "<name> version <ver> [<date>]" boot line in place
#  (length-preserving).  The FreeDOS KERNEL.SYS banner is compressed
#  (exeflat+UPX) and printed unconditionally by the 1.3 kernel, so it is
#  NOT byte-patchable - removing it needs a kernel rebuilt from source.
#  FreeDOS attribution and its GPL text stay in LICENSES/, third_party/,
#  docs/, and the Castalia About screen.  See scripts/rebrand-dos.py.
info "blanking the FreeCOM sign-on banner (COMMAND.COM)"
python3 "$ROOT/scripts/rebrand-dos.py" "$PAYLOAD/KERNEL.SYS" "$PAYLOAD/COMMAND.COM"

# ---- 2.6 Prefer the CASTALIA-rebuilt kernel, if present --------------
#  scripts/build-kernel.sh (run in the CI "DOS build" job, which has Open
#  Watcom) rebuilds the FreeDOS kernel from source with a CASTALIA-only
#  signon() and leaves it uncompressed.  When that build/KERNEL.SYS is
#  available it replaces the stock packed FreeDOS kernel, so the boot
#  banner is CASTALIA and nothing else.  Without it (a plain payload
#  fetch), the stock kernel is used and its banner still shows.
if [ -f "$ROOT/build/KERNEL.SYS" ]; then
    info "using the CASTALIA-rebuilt kernel (build/KERNEL.SYS)"
    cp "$ROOT/build/KERNEL.SYS" "$PAYLOAD/KERNEL.SYS"
fi

# ---- 3. Verify --------------------------------------------------------
missing=0
for f in KERNEL.SYS COMMAND.COM HIMEMX.EXE JEMM386.EXE MEM.EXE \
         FDISK.EXE FORMAT.EXE SYS.COM XCOPY.EXE SHSUCDX.COM CHKDSK.EXE \
         CTMOUSE.EXE UIDE.SYS; do
    if [ -s "$PAYLOAD/$f" ]; then
        info "OK   $f ($(stat -c %s "$PAYLOAD/$f") bytes)"
    else
        err "MISSING $f"
        missing=1
    fi
done
if [ -s "$BOOTSEC" ]; then
    info "OK   fdboot.bin (boot sector)"
else
    err "MISSING fdboot.bin"
    missing=1
fi

if [ "$missing" -ne 0 ]; then
    exit 1
fi
info "payload complete."
