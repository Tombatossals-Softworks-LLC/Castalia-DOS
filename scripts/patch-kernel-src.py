#!/usr/bin/env python3
# =====================================================================
#  patch-kernel-src.py  -  apply the CASTALIA source modifications to
#                          the FreeDOS 1.3 (build 2043) kernel tree
# ---------------------------------------------------------------------
#  Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
#  SPDX-License-Identifier: MIT  (this script)
#
#  scripts/build-kernel.sh unpacks the pinned FreeDOS kernel source
#  (ke2043s.zip) and calls this script with the source ROOT.  We apply a
#  small, well-marked set of GPLv2 §2 modifications that turn the stock
#  FreeDOS kernel into the *Castalia* kernel - a genuine derivative, not
#  just a renamed banner:
#
#    1. signon()        kernel/main.c   - one CASTALIA sign-on line only
#    2. OEM identity    hdr/version.h   - OEM_ID 0xFD (FreeDOS) -> 0xCA
#                       kernel/kernel.asm  (the resident VERSION resource)
#                       so INT 21h/AH=30h reports Castalia in BH.
#    3. resident global kernel/globals.h - castalia_boot_profile byte
#                       (init writes it, the INT 2Fh handler reads it).
#    4. CONFIG.SYS      kernel/config.c - a CASTALIA= directive that
#                       records the active boot profile in that byte.
#    5. identity API    kernel/int2f.asm - INT 2Fh AH=0CAh multiplex that
#                       reports the Castalia signature, build, edition,
#                       OEM id and active boot profile in registers.
#
#  Every change is idempotent (re-running is a no-op), anchored on unique
#  text in the pinned source, and fails LOUDLY if an anchor is gone (a
#  changed upstream would otherwise silently ship an unbranded kernel).
#
#  GPL: these are §2 modifications.  They are marked here and in
#  docs/LICENSE-STRATEGY.md §2.3.2a; the pinned ke2043s.zip plus this
#  script are the corresponding source for the rebuilt kernel.
#
#  The register-level contract of (2)/(4)/(5) is documented for callers
#  in docs/KERNEL.md and consumed by src/hwinfo and src/castalia.
#
#  Usage:  patch-kernel-src.py path/to/SOURCE/ke2043
#          (legacy: a path ending in kernel/main.c is also accepted and
#           resolved to its kernel-source root, so older callers work.)
#  Exit:   0 all patches applied (or already applied); 1 on any failure.
# =====================================================================
import os
import re
import sys

# ---- the CASTALIA kernel identity contract (keep in sync w/ docs) ----
CAST_OEMID     = 0xCA        # INT 21h AH=30h BH; INT 2Fh CA00h DL
CAST_SIG       = 0xCA5A      # INT 2Fh CA00h/CA01h BX confirmation word
CAST_BUILD     = 1           # INT 2Fh CA00h CX  (Castalia kernel build)
CAST_EDITION   = 0x01        # INT 2Fh CA00h DH  (01 = 386SX Edition)

CASTALIA_LINE  = 'CASTALIA DOS 386SX Edition  -  1.0 \\"Tombatossals\\"'


def fail(msg):
    sys.stderr.write("  patch-kernel: ERROR %s\n" % msg)
    raise SystemExit(1)


def note(msg):
    sys.stdout.write("  patch-kernel: %s\n" % msg)


def read(path):
    if not os.path.isfile(path):
        fail("missing source file: %s" % path)
    # newline="" keeps the file's CRLF line endings byte-for-byte.
    with open(path, "r", newline="") as fp:
        return fp.read()


def write(path, text):
    with open(path, "w", newline="") as fp:
        fp.write(text)


def eol_of(text):
    return "\r\n" if "\r\n" in text else "\n"


def require_once(text, anchor, path):
    n = text.count(anchor)
    if n == 0:
        fail("anchor not found in %s: %r" % (path, anchor))
    if n > 1:
        fail("anchor not unique (%d) in %s: %r" % (n, path, anchor))


def insert_after_line(text, anchor, new_lines, path):
    """Insert new_lines (list of str, no EOL) right after the unique line
    that contains `anchor`, preserving the file's EOL."""
    require_once(text, anchor, path)
    eol = eol_of(text)
    i = text.index(anchor)
    line_end = text.index("\n", i)          # end of the anchor's line (LF)
    # keep everything up to and including that LF, then splice our lines
    head = text[:line_end + 1]
    tail = text[line_end + 1:]
    block = "".join(l + eol for l in new_lines)
    return head + block + tail


# =====================================================================
#  1. signon() -> a single CASTALIA line   (kernel/main.c)
# =====================================================================
def patch_signon(ksrc):
    path = os.path.join(ksrc, "kernel", "main.c")
    src = read(path)
    if "CASTALIA DOS 386SX Edition" in src:
        note("main.c signon already rebranded")
        return
    fmt_start = 'printf("\\r%S"'
    fmt_end = '"\\n\\n%s",'
    i = src.find(fmt_start)
    if i < 0:
        fail("signon() printf not found in main.c")
    j = src.find(fmt_end, i)
    if j < 0:
        fail("signon() format tail '\\n\\n%s' not found in main.c")
    j_end = j + len(fmt_end)
    span = src[i:j_end]
    # Safety: the span we remove must really be the banner (names the
    # compiler/CPU tag), so we never clobber the wrong printf.
    if "WATCOMC" not in span and "Kernel compatibility" not in span:
        fail("signon() span does not look like the banner; refusing")
    # Keep the ORIGINAL arguments referenced (os_release, MAJOR/MINOR,
    # copyright) so the -we (warnings-as-errors) build keeps them "used";
    # only the format string is replaced.
    new_printf = 'printf("\\r\\n' + CASTALIA_LINE + '\\r\\n\\n",'
    src = src[:i] + new_printf + src[j_end:]

    # Stamp the boot tick.  signon() runs exactly once, unconditionally, at
    # boot, so it is the natural place to record 0040:006C for uptime.
    # peekl() comes from portab.h, which main.c already includes.
    eol = eol_of(src)
    open_body = "STATIC VOID signon()" + eol + "{" + eol
    require_once(src, open_body, path)
    src = src.replace(
        open_body,
        open_body +
        "  /* CASTALIA: stamp the BIOS tick so tools can report uptime. */"
        + eol +
        "  castalia_boot_tick = peekl(0, 0x46c);" + eol,
        1)

    write(path, src)
    note("main.c signon rebranded + boot tick stamped")


# =====================================================================
#  2. OEM identity 0xFD (FreeDOS) -> 0xCA (Castalia)
#     hdr/version.h (the #define) and kernel/kernel.asm (the resident
#     VERSION resource); INT 21h AH=30h returns this byte in BH.
# =====================================================================
def patch_oem(ksrc):
    vh = os.path.join(ksrc, "hdr", "version.h")
    src = read(vh)
    if "/* CASTALIA OEM" in src:
        note("version.h OEM_ID already Castalia")
    else:
        new = '#define OEM_ID          0x%02x    /* CASTALIA OEM id (was 0xfd FreeDOS) */' % CAST_OEMID
        src2 = re.sub(r'#define OEM_ID\s+0xfd\b[^\r\n]*', new, src)
        if src2 == src:
            fail("version.h OEM_ID 0xfd define not found")
        write(vh, src2)
        note("version.h OEM_ID -> 0x%02x (Castalia)" % CAST_OEMID)

    ka = os.path.join(ksrc, "kernel", "kernel.asm")
    src = read(ka)
    if "OEM_ID (CASTALIA" in src:
        note("kernel.asm Version_OemID already Castalia")
    else:
        new = 'Version_OemID               db 0x%02X     ; OEM_ID (CASTALIA, was 0xFD)' % CAST_OEMID
        src2 = re.sub(r'Version_OemID\s+db 0xFD\b[^\r\n]*', new, src)
        if src2 == src:
            fail("kernel.asm Version_OemID db 0xFD not found")
        write(ka, src2)
        note("kernel.asm Version_OemID -> 0x%02X (Castalia)" % CAST_OEMID)


# =====================================================================
#  3. resident global castalia_boot_profile   (kernel/globals.h)
#     Declared exactly like HaltCpuWhileIdle: defined in the MAIN unit
#     (inthndlr.c) so it is resident, and reachable from int2f.asm via
#     DGROUP.  init (config.c) writes it; the INT 2Fh handler reads it.
# =====================================================================
def patch_global(ksrc):
    path = os.path.join(ksrc, "kernel", "globals.h")
    src = read(path)
    if "castalia_boot_profile" in src:
        note("globals.h castalia_boot_profile already present")
        return
    anchor = "GLOBAL BYTE ASM HaltCpuWhileIdle;"
    src = insert_after_line(
        src, anchor,
        ["/* CASTALIA: active boot profile recorded by the CASTALIA= "
         "CONFIG.SYS directive (0 = unset). */",
         "GLOBAL BYTE ASM castalia_boot_profile;",
         "/* CASTALIA: BIOS tick (0040:006C) captured at sign-on, so tools "
         "can report a true uptime. */",
         "GLOBAL ULONG ASM castalia_boot_tick;"],
        path)
    write(path, src)
    note("globals.h: added resident castalia_boot_profile + castalia_boot_tick")


# =====================================================================
#  3b. init-mod.h - make the resident symbols visible to the INIT code
#      main.c and config.c include init-mod.h, NOT globals.h: the init
#      modules get their own `extern ... DOSFAR ASM` declarations there
#      (see ReturnAnyDosVersionExpected / HaltCpuWhileIdle).  Without
#      this the kernel build fails with
#        config.c: Error! E1011: Symbol 'castalia_boot_profile' ...
# =====================================================================
def patch_initmod(ksrc):
    path = os.path.join(ksrc, "kernel", "init-mod.h")
    src = read(path)
    if "castalia_boot_profile" in src:
        note("init-mod.h castalia declarations already present")
        return
    anchor = "extern BYTE DOSFAR ASM HaltCpuWhileIdle;"
    src = insert_after_line(
        src, anchor,
        ["/* CASTALIA: resident state written by the init code (config.c sets",
         " * the profile from CASTALIA=, main.c stamps the boot tick). */",
         "extern BYTE DOSFAR ASM castalia_boot_profile;",
         "extern ULONG DOSFAR ASM castalia_boot_tick;"],
        path)
    write(path, src)
    note("init-mod.h: declared castalia_boot_profile + castalia_boot_tick")


# =====================================================================
#  4. CASTALIA= CONFIG.SYS directive   (kernel/config.c)
#     Records the active boot profile number (1..8) in the resident
#     castalia_boot_profile byte.  Registered in commands[] at pass 1.
# =====================================================================
def patch_config(ksrc):
    path = os.path.join(ksrc, "kernel", "config.c")
    src = read(path)
    if "CfgCastalia" in src:
        note("config.c CASTALIA= directive already present")
        return
    eol = eol_of(src)

    # (a) prototype, just before the commands[] table that references it
    proto_anchor = "STATIC struct table commands[] = {"
    require_once(src, proto_anchor, path)
    proto = ("STATIC VOID CfgCastalia(BYTE * pLine);"
             "   /* CASTALIA: record active boot profile */" + eol + eol)
    src = src.replace(proto_anchor, proto + proto_anchor, 1)

    # (b) table entry, right after the ANYDOS row
    src = insert_after_line(
        src, '{"ANYDOS", 1, SetAnyDos},',
        ['  {"CASTALIA", 1, CfgCastalia}, /* CASTALIA: active boot profile */'],
        path)

    # (c) the handler itself, right after SetAnyDos()
    setanydos = ("STATIC VOID SetAnyDos(BYTE * pLine)" + eol +
                 "{" + eol +
                 "  UNREFERENCED_PARAMETER(pLine);" + eol +
                 "  ReturnAnyDosVersionExpected = TRUE;" + eol +
                 "}" + eol)
    require_once(src, setanydos, path)
    handler = (eol +
               "/*" + eol +
               "   CASTALIA DOS extension:  CASTALIA=n" + eol +
               "       Records the active boot profile number (1..8) in the" + eol +
               "       resident castalia_boot_profile byte so the rest of the" + eol +
               "       system (INT 2Fh AH=0CAh) can read which profile booted." + eol +
               "       Unknown/empty values leave it unset (0).  Never fatal." + eol +
               "*/" + eol +
               "STATIC VOID CfgCastalia(BYTE * pLine)" + eol +
               "{" + eol +
               "  COUNT profile;" + eol +
               "  if (GetNumArg(pLine, &profile))" + eol +
               "    castalia_boot_profile = (BYTE)profile;" + eol +
               "}" + eol)
    src = src.replace(setanydos, setanydos + handler, 1)

    write(path, src)
    note("config.c: added CASTALIA= directive (proto + table + handler)")


# =====================================================================
#  5. INT 2Fh AH=0CAh identity multiplex   (kernel/int2f.asm)
#     A Castalia extension in the application AH range.  The kernel 2F
#     handler is the bottom of the chain and iret's for functions it does
#     not own, so intercepting 0CAh at the top affects nothing else.
# =====================================================================
def patch_int2f(ksrc):
    path = os.path.join(ksrc, "kernel", "int2f.asm")
    src = read(path)
    if "CastaliaMux" in src:
        note("int2f.asm identity multiplex already present")
        return
    eol = eol_of(src)

    # (a) make the resident profile byte visible to this asm module
    src = insert_after_line(
        src, "extern _HaltCpuWhileIdle",
        ["            extern _castalia_boot_profile ; CASTALIA boot profile byte",
         "            extern _castalia_boot_tick    ; CASTALIA boot BIOS tick"],
        path)

    # (b) intercept AH=0CAh at the very top of the handler, before the
    #     stock "Network interrupt?" check.
    src = insert_after_line(
        src, "sti                             ; Enable interrupts",
        ["                cmp     ah,0CAh                 ; CASTALIA identity multiplex?",
         "                je      CastaliaMux             ; a Castalia DOS extension"],
        path)

    # (c) the handler body, placed right after "retf 2" (a return, so no
    #     fall-through), just before the WinIdle label.  Each path iret's.
    block = [
        "",
        "; ------------------------------------------------------------------",
        ";  CASTALIA kernel identity multiplex  (INT 2Fh, AH=0CAh)",
        ";  A Castalia DOS extension - NOT part of stock FreeDOS.  See",
        ";  docs/KERNEL.md for the register contract.",
        ";    AX=CA00h -> AL=FFh, BX=CA5Ah (sig), CX=build,",
        ";               DH=edition (01=386SX), DL=OEM id (0CAh)",
        ";    AX=CA01h -> AL=FFh, BX=CA5Ah (sig), CL=active boot profile (0=unset)",
        ";    AX=CA02h -> AL=FFh, BX=CA5Ah (sig), CX:DX=BIOS tick stamped at boot",
        ";               (uptime = current 0040:006C minus this)",
        ";    other AL -> returned unhandled (AL left unchanged)",
        "; ------------------------------------------------------------------",
        "CastaliaMux:",
        "                cmp     al,0                    ; AL=00 identity?",
        "                je      CastaliaId",
        "                cmp     al,1                    ; AL=01 active boot profile?",
        "                je      CastaliaProfile",
        "                cmp     al,2                    ; AL=02 boot tick (uptime)?",
        "                je      CastaliaUptime",
        "                iret                            ; unknown Castalia subfunction",
        "CastaliaId:",
        "                mov     al,0FFh                 ; installed marker",
        "                mov     bx,0%04Xh               ; CASTALIA signature" % CAST_SIG,
        "                mov     cx,%d                    ; CASTALIA kernel build number" % CAST_BUILD,
        "                mov     dx,0%02X%02Xh              ; DH=edition, DL=OEM id" % (CAST_EDITION, CAST_OEMID),
        "                iret",
        "CastaliaProfile:",
        "                push    ds",
        "                mov     ds,[cs:_DGROUP_]        ; reach resident data",
        "                mov     cl,[_castalia_boot_profile]",
        "                pop     ds",
        "                mov     ch,0",
        "                mov     al,0FFh                 ; installed marker",
        "                mov     bx,0%04Xh               ; CASTALIA signature" % CAST_SIG,
        "                iret",
        "CastaliaUptime:",
        "                push    ds",
        "                mov     ds,[cs:_DGROUP_]        ; reach resident data",
        "                mov     dx,[_castalia_boot_tick]     ; low  word",
        "                mov     cx,[_castalia_boot_tick+2]   ; high word",
        "                pop     ds",
        "                mov     al,0FFh                 ; installed marker",
        "                mov     bx,0%04Xh               ; CASTALIA signature" % CAST_SIG,
        "                iret",
    ]
    src = insert_after_line(
        src, "retf    2                       ; Return far", block, path)

    write(path, src)
    note("int2f.asm: added INT 2Fh AH=0CAh identity multiplex")


def resolve_root(arg):
    # Accept either the kernel source root or a legacy .../kernel/main.c.
    if arg.endswith("main.c"):
        root = os.path.dirname(os.path.dirname(arg))
        note("legacy arg: using kernel root %s" % root)
        return root
    return arg


def main(argv):
    if len(argv) != 2:
        sys.stderr.write("usage: patch-kernel-src.py path/to/SOURCE/ke2043\n")
        return 1
    ksrc = resolve_root(argv[1])
    if not os.path.isfile(os.path.join(ksrc, "kernel", "main.c")):
        fail("not a kernel source root (no kernel/main.c): %s" % ksrc)

    patch_global(ksrc)
    patch_initmod(ksrc)     # must precede the init-code patches (main/config)
    patch_signon(ksrc)
    patch_oem(ksrc)
    patch_config(ksrc)
    patch_int2f(ksrc)

    note("all CASTALIA kernel modifications applied")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
