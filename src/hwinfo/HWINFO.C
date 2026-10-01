/* ===================================================================
 * HWINFO.C  -  CASTALIA DOS Hardware Diagnostics  (HWINFO.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Reports what the machine and DOS look like: CPU class, math
 * coprocessor, conventional/extended memory, XMS/EMS, VGA, mouse driver,
 * CD-ROM (MSCDEX), the sound environment, the reported DOS version, free
 * disk space, and the booted Castalia profile.
 *
 * Detection here is deliberately CONSERVATIVE and honest: it uses only
 * safe BIOS interrupts, DOS calls, and the multiplex interface - no blind
 * calls into possibly-absent drivers, and no fragile CPU-speed guessing.
 * CPU class and the coprocessor come from the shared CPUDET module,
 * whose EFLAGS/CPUID and FNINIT/FNSTSW probes are machine code
 * byte-encoded inside #pragma aux (see CPUDET.C and docs/DIAGNOSTICS.md);
 * CPUID runs only after the ID-bit toggle proves it exists.
 *
 * Build (Open Watcom; the Makefile rule is authoritative):
 *   wcl -0 -bt=dos -ml -os hwinfo.c ..\common\ui.c ..\common\cpudet.c
 *       ..\common\xmsinfo.c ..\common\viddet.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <fcntl.h>
#if defined(__WATCOMC__) || defined(__TURBOC__)
#include <io.h>
#else
#include <unistd.h>            /* host syntax-check only */
#endif
#include "../common/UI.H"
#include "../common/CPUDET.H"
#include "../common/XMSINFO.H"
#include "../common/VIDDET.H"

#ifndef MK_FP
#define MK_FP(seg, ofs) \
    ((void far *)(((unsigned long)(seg) << 16) | (unsigned)(ofs)))
#endif

/* --- Safe detection primitives -------------------------------------- */

/* Conventional memory in KB (BIOS INT 12h). */
static unsigned conv_kb(void)
{
    union REGS r;
    int86(0x12, &r, &r);
    return r.x.ax;
}

/* DOS version via INT 21h AH=30h -> major.minor. */
static void dos_version(int *major, int *minor)
{
    union REGS r;
    r.h.ah = 0x30;
    int86(0x21, &r, &r);
    *major = r.h.al;
    *minor = r.h.ah;
}

/* CASTALIA kernel identity via INT 2Fh AX=CA00h (see docs/KERNEL.md).
 * Returns the Castalia kernel build number (>=1) when the CASTALIA kernel
 * answers with its signature word in BX; 0 on a stock/FreeDOS-compatible
 * kernel that ignores the call.  edition/oem may be NULL. */
static int castalia_kernel(int *edition, int *oem)
{
    union REGS r;
    r.x.ax = 0xCA00;
    int86(0x2F, &r, &r);
    if (r.h.al != 0xFF || r.x.bx != 0xCA5A)
        return 0;                       /* not the CASTALIA kernel */
    if (edition) *edition = r.h.dh;
    if (oem)     *oem     = r.h.dl;
    return r.x.cx;                      /* build number */
}

/* Free bytes on the default drive (INT 21h AH=36h, DL=0). */
static unsigned long disk_free(void)
{
    union REGS r;
    r.h.ah = 0x36;
    r.h.dl = 0x00;               /* default drive */
    int86(0x21, &r, &r);
    if (r.x.ax == 0xFFFF)        /* invalid drive */
        return 0UL;
    return (unsigned long)r.x.ax *      /* sectors per cluster */
           (unsigned long)r.x.bx *      /* free clusters       */
           (unsigned long)r.x.cx;       /* bytes per sector    */
}

/* VGA present?  INT 10h AX=1A00h returns AL=1Ah on VGA BIOSes. */
/* XMS present?  INT 2Fh AX=4300h -> AL=80h. */
static int has_xms(void)
{
    union REGS r;
    r.x.ax = 0x4300;
    int86(0x2F, &r, &r);
    return (r.h.al == 0x80);
}

/* EMS present?  Safely open the EMS device "EMMXXXX0"; the C runtime
 * handles the DOS call and segments.  If the driver is loaded, the open
 * succeeds. */
static int has_ems(void)
{
    int h = open("EMMXXXX0", O_RDONLY);
    if (h < 0)
        return 0;
    close(h);
    return 1;
}

/* Number of CD-ROM drives via the MSCDEX multiplex (INT 2Fh AX=1500h).
 * Safe: INT 2Fh always has a handler; BX comes back 0 if no MSCDEX. */
static int cdrom_drives(void)
{
    union REGS r;
    r.x.ax = 0x1500;
    r.x.bx = 0x0000;
    int86(0x2F, &r, &r);
    return r.x.bx;
}

/* Read an interrupt vector from the IVT at 0000:vec*4. */
static void get_ivt(int vec, unsigned *seg, unsigned *off)
{
    unsigned far *p = (unsigned far *)MK_FP(0, (unsigned)(vec * 4));
    *off = p[0];
    *seg = p[1];
}

/* Mouse driver present?  Check the INT 33h vector is set and not a bare
 * IRET, then (only then) ask the driver for its status. */
static int has_mouse(int *buttons)
{
    unsigned seg, off;
    unsigned char far *entry;
    union REGS r;

    *buttons = 0;
    get_ivt(0x33, &seg, &off);
    if (seg == 0 && off == 0)
        return 0;
    entry = (unsigned char far *)MK_FP(seg, off);
    if (*entry == 0xCF)          /* IRET -> stub, no real driver */
        return 0;
    r.x.ax = 0x0000;
    int86(0x33, &r, &r);
    if (r.x.ax == 0xFFFF) {
        *buttons = r.x.bx;
        return 1;
    }
    return 0;
}

/* BIOS equipment word (INT 11h): coprocessor bit (1) + serial-port count
 * (bits 9-11) + parallel-port count (bits 14-15). */
static unsigned equip_word(void)
{
    union REGS r;
    int86(0x11, &r, &r);
    return r.x.ax;
}

/* CPU / FPU detection lives in the shared module src/common/CPUDET.C
 * (compiled with 386 instructions under Open Watcom; portable fallback
 * elsewhere).  CASTMARK uses the same module - one probe, one truth. */

/* --- Report rendering ----------------------------------------------- */

static void put_row(int y, const char *label, const char *value)
{
    ui_puts(6, y, label, UI_ATTR(C_LGRAY, C_BLUE));
    ui_puts(30, y, value, UI_ATTR(C_YELLOW, C_BLUE));
}

int main(void)
{
    char v[64];
    int dmaj, dmin, mbtn;
    unsigned c_kb, xver = 0;
    unsigned long freeb;
    int cds;
    const char *env;
    int y;

    ui_init();
    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA DOS", A_TITLE);
    ui_puts(SCR_W - 22, 0, "Hardware Diagnostics", UI_ATTR(C_WHITE, C_BLUE));

    ui_box(3, 2, 74, 20, A_FRAME);
    ui_puts(5, 2, " System Report ", A_TITLE);

    y = 4;

    /* CPU: refined where possible, honest baseline otherwise. */
    cpu_describe(v);
    put_row(y++, "Processor", v);
    sprintf(v, "%s (%s)",
            fpu_present() ? "present" : "not detected",
            fpu_probe_kind() ? "FNSTSW probe" : "BIOS flag");
    put_row(y++, "Math coprocessor", v);
    {
        unsigned eq = equip_word();
        sprintf(v, "%u serial (COM), %u parallel (LPT)",
                (unsigned)((eq >> 9) & 7), (unsigned)((eq >> 14) & 3));
        put_row(y++, "Ports", v);
    }

    c_kb = conv_kb();
    sprintf(v, "%u KB total", c_kb);
    put_row(y++, "Conventional memory", v);

    /* Ask the XMS driver, not the BIOS: once HIMEMX owns extended memory
     * it answers INT 15h AH=88h with 0 on purpose (see XMSINFO.C). */
    mem_ext_describe(v);
    put_row(y++, "Extended memory", v);

    if (xms_version(&xver) && xver != 0)
        sprintf(v, "driver present, version %x.%02x",
                (xver >> 8) & 0xFF, xver & 0xFF);
    else
        strcpy(v, has_xms() ? "driver present" : "not available");
    put_row(y++, "XMS (extended)", v);
    put_row(y++, "EMS (expanded)", has_ems() ? "available" : "not available");
    put_row(y++, "Video adapter", vid_name(vid_class()));

    if (has_mouse(&mbtn)) {
        sprintf(v, "driver present (%d buttons)", mbtn);
        put_row(y++, "Mouse", v);
    } else {
        put_row(y++, "Mouse", "no driver loaded");
    }

    cds = cdrom_drives();
    if (cds > 0)
        sprintf(v, "%d drive(s) via MSCDEX/SHSUCDX", cds);
    else
        strcpy(v, "none (boot the CD-ROM profile to enable)");
    put_row(y++, "CD-ROM", v);

    env = getenv("BLASTER");
    put_row(y++, "Sound (BLASTER)", (env && env[0]) ? env : "not set");

    {
        int kbuild, ked = 0, koem = 0;
        kbuild = castalia_kernel(&ked, &koem);
        if (kbuild)
            sprintf(v, "CASTALIA build %d  (OEM %02Xh, ed %d)", kbuild, koem, ked);
        else
            strcpy(v, "FreeDOS-compatible (stock kernel)");
        put_row(y++, "Kernel", v);
    }

    dos_version(&dmaj, &dmin);
    sprintf(v, "reports %d.%02d (MS-DOS compatible)", dmaj, dmin);
    put_row(y++, "DOS version", v);

    freeb = disk_free();
    sprintf(v, "%lu KB free on current drive", freeb / 1024UL);
    put_row(y++, "Disk space", v);

    env = getenv("CASTPROFILE");
    put_row(y++, "Boot profile", (env && env[0]) ? env : "(unknown)");

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(2, SCR_H - 1,
        "Press any key to return.   Some items DOS cannot detect exactly "
        "- see HELP.", A_STATUS);

    ui_getkey();
    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
