#!/usr/bin/env bash
# =====================================================================
#  build-kernel.sh  -  rebuild the FreeDOS kernel with a CASTALIA banner
# ---------------------------------------------------------------------
#  Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
#  SPDX-License-Identifier: MIT  (this script; the kernel itself is GPLv2+)
#
#  The shipped FreeDOS KERNEL.SYS is packed (exeflat+UPX) and its 1.3
#  signon() prints "FreeDOS kernel ... WATCOMC ..." + the GPL copyright
#  unconditionally, so the banner cannot be removed by byte-patching.
#  This rebuilds the FreeDOS 1.3 kernel (build 2043) from its official
#  source with one marked source change - signon() rewritten to a single
#  CASTALIA line (scripts/patch-kernel-src.py) - and produces an
#  UNCOMPRESSED build/KERNEL.SYS whose only sign-on is CASTALIA.
#
#  Uncompressed is deliberate: no UPX dependency, and the FreeDOS boot
#  sector loads a compressed or uncompressed kernel identically (same
#  LOADSEG=0x60 contract).  The extra ~40 KB fits the 1.44 MB floppy.
#
#  Toolchain (all present in the CI "DOS build" job + apt): Open Watcom
#  (wcc/wlink/wmake), gcc (host utils like exeflat), nasm, GNU make.
#
#  GPL: this is a §2 modification.  The change is marked here and in
#  docs/LICENSE-STRATEGY.md 2.3.2a; the pinned upstream source
#  (ke2043s.zip) plus scripts/patch-kernel-src.py are the corresponding
#  source for the rebuilt kernel.
#
#  Usage:   scripts/build-kernel.sh [--cache DIR]
#  Output:  build/KERNEL.SYS
#  Exit:    0 success; 1 build/verify error; 2 missing tool.
# =====================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

CACHE="$ROOT/.payload-cache"
[ "${1:-}" = "--cache" ] && CACHE="$2"

SRC_URL="https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/dos/kernel/2043/ke2043s.zip"
WORK="$ROOT/build/kernel-build"
OUT="$ROOT/build/KERNEL.SYS"
KSRC="$WORK/SOURCE/ke2043"

err()  { printf 'ERR  %s\n' "$*" >&2; }
info() { printf '  %s\n' "$*"; }
ok()   { printf '  OK   %s\n' "$*"; }

# ---- tool checks ----------------------------------------------------
for t in gcc nasm make unzip curl python3; do
    command -v "$t" >/dev/null 2>&1 || { err "missing tool: $t"; exit 2; }
done
if ! command -v wmake >/dev/null 2>&1; then
    err "Open Watcom (wmake) not found; load the Open Watcom environment"
    err "(the CI 'DOS build' job does this via open-watcom/setup-watcom)."
    exit 2
fi

# ---- fetch the kernel source (cacheable) ----------------------------
mkdir -p "$CACHE" "$ROOT/build"
if [ ! -s "$CACHE/ke2043s.zip" ]; then
    info "downloading ke2043s.zip (FreeDOS 1.3 kernel source)"
    curl -sL --fail --max-time 600 -o "$CACHE/ke2043s.zip" "$SRC_URL"
else
    info "cached: ke2043s.zip"
fi
#  "Pinned" means pinned: the same digest fetch-payload.sh checks.  This
#  source becomes the kernel every Castalia machine boots.
want=$(awk '{ sub(/\r$/, "") } $1 !~ /^#/ && $2 == "ke2043s.zip" { print $1 }' \
           "$ROOT/scripts/payload.sha256")
got=$(sha256sum "$CACHE/ke2043s.zip" | cut -d' ' -f1)
if [ -z "$want" ] || [ "$got" != "$want" ]; then
    err "ke2043s.zip does not match its pinned digest in scripts/payload.sha256"
    err "  expected ${want:-<none>}"
    err "  got      $got"
    exit 1
fi
ok "ke2043s.zip matches its pinned digest"

# ---- fresh work tree ------------------------------------------------
rm -rf "$WORK"
mkdir -p "$WORK"
info "extracting the kernel source"
unzip -q "$CACHE/ke2043s.zip" -d "$WORK"
[ -f "$KSRC/kernel/main.c" ] || { err "unexpected source layout"; exit 1; }

# ---- apply the CASTALIA source modifications ------------------------
#  signon + OEM id (0xCA) + resident boot-profile byte + CASTALIA=
#  CONFIG.SYS directive + the INT 2Fh AH=0CAh identity multiplex.
#  See scripts/patch-kernel-src.py and docs/KERNEL.md.
python3 "$ROOT/scripts/patch-kernel-src.py" "$KSRC"

# ---- verify the source patches took (fail before wasting a build) ---
info "verifying the CASTALIA source modifications"
check_src() {  # file marker human-description
    if grep -qa -- "$2" "$1"; then
        ok "$3"
    else
        err "source patch missing: $3 ($2 not in $1)"
        exit 1
    fi
}
check_src "$KSRC/kernel/main.c"    "CASTALIA DOS 386SX Edition" "signon rebranded"
check_src "$KSRC/hdr/version.h"    "CASTALIA OEM id"            "OEM id 0xCA (version.h)"
check_src "$KSRC/kernel/kernel.asm" "OEM_ID (CASTALIA"          "OEM id 0xCA (kernel.asm)"
check_src "$KSRC/kernel/globals.h" "castalia_boot_profile"      "resident boot-profile byte"
check_src "$KSRC/kernel/globals.h" "castalia_boot_tick"         "resident boot-tick stamp"
check_src "$KSRC/kernel/init-mod.h" "castalia_boot_profile"     "init-code declarations"
check_src "$KSRC/kernel/main.c"    "castalia_boot_tick"         "boot tick stamped in signon()"
check_src "$KSRC/kernel/config.c"  "CfgCastalia"                "CASTALIA= directive"
check_src "$KSRC/kernel/int2f.asm" "CastaliaMux"                "INT 2Fh identity multiplex"
check_src "$KSRC/kernel/int2f.asm" "CastaliaUptime"             "INT 2Fh CA02h uptime call"

# ---- invariant: init code only names symbols it can actually see ----
#  main.c and config.c include init-mod.h, NOT globals.h.  A Castalia
#  symbol declared only in globals.h compiles fine everywhere else and
#  then fails the real build with
#     config.c: Error! E1011: Symbol 'castalia_...' has not been declared
#  (exactly how the first cut of this patch broke CI).  Catch it here,
#  before Open Watcom is even invoked.
info "verifying init-code symbol visibility"
if ! python3 - "$KSRC/kernel" <<'PYEOF'
import re, sys
kdir = sys.argv[1].rstrip('/') + '/'
declared = set(re.findall(r'castalia_\w+', open(kdir + 'init-mod.h').read()))
missing = {}
for f in ('main.c', 'config.c'):
    used = set(re.findall(r'castalia_\w+', open(kdir + f).read()))
    gap = used - declared
    if gap:
        missing[f] = sorted(gap)
if missing:
    for f, syms in missing.items():
        sys.stderr.write("  %s uses undeclared Castalia symbols: %s\n"
                         % (f, ', '.join(syms)))
    sys.stderr.write("  (declare them in kernel/init-mod.h, as FreeDOS does\n"
                     "   for ReturnAnyDosVersionExpected / HaltCpuWhileIdle)\n")
    raise SystemExit(1)
raise SystemExit(0)
PYEOF
then
    err "init-code symbol visibility check failed"
    exit 1
fi
ok "init code only names symbols declared in init-mod.h"

# ---- relax "treat warnings as errors" -------------------------------
#  The 2043 source predates our Open Watcom snapshot; -we would turn any
#  new, pedantic warning from a newer compiler into a build failure.  We
#  are rebuilding a known-good codebase (not developing it), so drop -we
#  (keeping -wx's high warning level) to stay robust across OW versions.
sed -i 's/-we//g' "$KSRC"/mkfiles/*.mak

# ---- build: uncompressed (XUPX=), 8086, FAT32 (matches FloppyEdition)
#  COMPILER=owlinux is auto-selected on Linux; sub-directory builds use
#  Open Watcom wmake, host utilities (exeflat) build with gcc, country.sys
#  assembles with nasm.  A command-line XUPX= overrides the makefile's
#  default so exeflat leaves the kernel uncompressed.
info "building the kernel (make all, uncompressed)"
( cd "$KSRC" && make all XUPX= XCPU=86 XFAT=32 )

# ---- locate + verify -------------------------------------------------
KS="$KSRC/bin/kernel.sys"
[ -f "$KS" ] || KS="$KSRC/kernel/kernel.sys"
[ -f "$KS" ] || { err "kernel.sys was not produced by the build"; exit 1; }

# Uncompressed + rebranded: our banner is plain ASCII in the image, and
# the old "Kernel compatibility ... WATCOMC" signon text is gone.
if ! grep -qa "CASTALIA DOS 386SX Edition" "$KS"; then
    err "the built kernel does not carry the CASTALIA banner"
    err "(build may have produced a stale or still-packed kernel)."
    exit 1
fi
if grep -qa "Kernel compatibility" "$KS"; then
    err "the built kernel still carries the FreeDOS 'Kernel compatibility'"
    err "sign-on; the signon() patch did not take effect."
    exit 1
fi

cp "$KS" "$OUT"
ok "build/KERNEL.SYS built ($(stat -c %s "$OUT") bytes, uncompressed, CASTALIA sign-on)"
