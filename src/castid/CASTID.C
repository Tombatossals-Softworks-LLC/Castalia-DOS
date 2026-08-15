/* ===================================================================
 * CASTID.C  -  CASTALIA SIGNATURE  (CASTID.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * The Castalia "system signature card": a one-screen identity readout
 * that proves the running kernel is genuinely CASTALIA's.  It queries
 * the Castalia kernel identity multiplex (INT 2Fh AH=0CAh, see
 * docs/KERNEL.md) for the build number, edition, OEM id and the live
 * boot profile, and rounds it out with CPU, memory and a ticking clock.
 *
 * On a stock/FreeDOS kernel the identity call is ignored and the card
 * says so honestly - it never pretends.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os castid.c ..\common\ui.c ..\common\cpudet.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include "../common/UI.H"
#include "../common/CPUDET.H"
#include "../common/XMSINFO.H"

/* ---- Castalia kernel identity (INT 2Fh AH=0CAh) -------------------- */

/* CA00h: identity.  Returns the Castalia build number (>=1), or 0 on a
 * stock kernel.  edition/oem receive DH/DL when present. */
static int cast_identity(int *edition, int *oem)
{
    union REGS r;
    r.x.ax = 0xCA00;
    int86(0x2F, &r, &r);
    if (r.h.al != 0xFF || r.x.bx != 0xCA5A)
        return 0;
    if (edition) *edition = r.h.dh;
    if (oem)     *oem     = r.h.dl;
    return r.x.cx;
}

/* CA01h: active boot profile code (1..8), or 0 if unset / not Castalia. */
static int cast_profile(void)
{
    union REGS r;
    r.x.ax = 0xCA01;
    int86(0x2F, &r, &r);
    if (r.h.al != 0xFF || r.x.bx != 0xCA5A)
        return 0;
    return r.h.cl;
}

/* CA02h: the BIOS tick the kernel stamped at sign-on.  *ok = 0 on a stock
 * kernel, which simply ignores the call. */
static unsigned long cast_boot_tick(int *ok)
{
    union REGS r;
    r.x.ax = 0xCA02;
    int86(0x2F, &r, &r);
    if (r.h.al != 0xFF || r.x.bx != 0xCA5A) {
        *ok = 0;
        return 0UL;
    }
    *ok = 1;
    return ((unsigned long)r.x.cx << 16) | (unsigned long)r.x.dx;
}

/* "H h MM m SS s" since the kernel signed on.  ui_ticks() IS the BIOS tick
 * counter, so uptime is just the difference at 18.2 ticks per second. */
static void uptime_str(char *out)
{
    int ok = 0;
    unsigned long boot, now, secs;

    boot = cast_boot_tick(&ok);
    if (!ok) {
        strcpy(out, "(kernel does not report)");
        return;
    }
    now = ui_ticks();
    if (now < boot) {                   /* BIOS resets the tick at midnight */
        strcpy(out, "(counter wrapped at midnight)");
        return;
    }
    secs = (now - boot) * 10UL / 182UL;
    sprintf(out, "%lu h %02lu m %02lu s",
            secs / 3600UL, (secs / 60UL) % 60UL, secs % 60UL);
}

static const char *profile_name(int code)
{
    static const char *names[9] = {
        "(unset)", "CLEAN", "XMS", "EMS", "CDROM",
        "WIN3X", "DIAG", "SAFE", "PROMPT"
    };
    if (code < 1 || code > 8)
        return names[0];
    return names[code];
}

/* ---- DOS / hardware probes (documented, portable INT calls) -------- */

static void dos_version(int *major, int *minor, int *oem)
{
    union REGS r;
    r.h.ah = 0x30;
    int86(0x21, &r, &r);
    *major = r.h.al;
    *minor = r.h.ah;
    *oem   = r.h.bh;                 /* OEM vendor byte */
}

static unsigned conv_kb(void)       /* INT 12h -> conventional KB */
{
    union REGS r;
    int86(0x12, &r, &r);
    return r.x.ax;
}

static void get_date(int *y, int *mo, int *d)
{
    union REGS r;
    r.h.ah = 0x2A;
    int86(0x21, &r, &r);
    *y  = r.x.cx;
    *mo = r.h.dh;
    *d  = r.h.dl;
}

static void get_time(int *h, int *mi, int *s)
{
    union REGS r;
    r.h.ah = 0x2C;
    int86(0x21, &r, &r);
    *h  = r.h.ch;
    *mi = r.h.cl;
    *s  = r.h.dh;
}

/* ---- The card ------------------------------------------------------ */

#define CX0 14                      /* card left column  */
#define CY0 2                       /* card top row      */
#define CW  52                      /* card width        */
#define CH  20                      /* card height       */

static void label(int row, const char *k, const char *val, unsigned char va)
{
    unsigned char ka = UI_ATTR(C_DGRAY, C_LGRAY);
    ui_puts(CX0 + 3,  CY0 + row, k, ka);
    /* Clip to the card's inner right edge.  A value that overflowed used
     * to run out onto the blue desktop, which reads as a rendering
     * fault rather than as long text. */
    ui_putlim(CX0 + 17, CY0 + row, val, CW - 19, va);
}

static void draw_frame(int build, int edition)
{
    unsigned char body = A_PANEL;
    int i;

    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA SIGNATURE", A_TITLE);
    ui_puts(SCR_W - 22, 0, "The measure of a keep", A_HINT);

    ui_fill(CX0, CY0, CW, CH, ' ', body);
    ui_box(CX0, CY0, CW, CH, A_PANEL);

    /* A row of battlements across the header for flavour. */
    ui_fill(CX0 + 1, CY0, CW - 2, 1, ' ', A_PANELHDR);
    for (i = CX0 + 2; i < CX0 + CW - 2; i += 2)
        ui_putc(i, CY0, (char)0xDB, A_PANELHDR);
    if (build)
        ui_puts(CX0 + 3, CY0, " CASTALIA DOS - the fortress kernel ", A_PANELHDR);
    else
        ui_puts(CX0 + 3, CY0, " CASTALIA DOS - stock DOS kernel ", A_PANELHDR);

    (void)edition;
}

/* ---- The giant --------------------------------------------------- *
 * Tombatossals ("hill-toppler") is the giant of Castello folklore who
 * moved the mountains to open the plain - and the codename of 1.0.  He
 * turns up if you knock on the card with T.  Any key sends him home. */
static void wake_the_giant(void)
{
    static const char *fig[] = {
        "        ,---.        ",
        "       ( o o )       ",
        "        \\ ^ /        ",
        "     ,---'-'---,     ",
        "    /  |     |  \\    ",
        "   '   |     |   '   ",
        "       |     |       ",
        "      _|     |_      "
    };
    int w = 46, h = 16, x = (SCR_W - w) / 2, y = 4;
    int rows = (int)(sizeof(fig) / sizeof(fig[0]));
    int i;
    unsigned char body = A_PANEL;
    unsigned char skin = UI_ATTR(C_BROWN, C_LGRAY);

    ui_fill(x, y, w, h, ' ', body);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " Tombatossals ", A_PANELHDR);

    for (i = 0; i < rows; i++)
        ui_puts(x + 12, y + 2 + i, fig[i], skin);

    ui_puts(x + 4, y + 11, "He toppled the hills to open the plain,",
            body);
    ui_puts(x + 4, y + 12, "and left the stone for us to build with.",
            body);
    ui_puts(x + 4, y + 14, "-  Castello folklore, and our 1.0",
            UI_ATTR(C_BROWN, C_LGRAY));

    (void)ui_getkey();
}

/* Paint everything that does not tick.  Called at start and again after
 * the giant has been and gone. */
static void paint_card(void)
{
    char buf[64];
    char cpu[48];
    int build, edition = 0, oem = 0;
    int dmaj = 0, dmin = 0, doem = 0, prof;
    unsigned char amber = UI_ATTR(C_BROWN,  C_LGRAY);
    unsigned char strong = UI_ATTR(C_BLACK, C_LGRAY);
    unsigned char good  = UI_ATTR(C_GREEN,  C_LGRAY);

    build   = cast_identity(&edition, &oem);
    prof    = cast_profile();
    dos_version(&dmaj, &dmin, &doem);
    /* The card's value column is CW-19 = 33 wide; the long form is 36
     * and lost its tail on the reference 386SX. */
    cpu_describe_short(cpu);

    draw_frame(build, edition);

    /* Product line. */
    ui_puts(CX0 + 3, CY0 + 2,
            "CASTALIA DOS 386SX Edition", strong);
    ui_puts(CX0 + 3, CY0 + 3,
            "1.0 \"Tombatossals\"", amber);

    ui_hline(CX0 + 1, CY0 + 4, CW - 2, A_PANEL);

    /* Kernel identity - the point of this whole tool. */
    if (build) {
        sprintf(buf, "CASTALIA  build %d  (edition %02Xh)", build, edition);
        label(5, "Kernel", buf, good);
        sprintf(buf, "%02Xh  (Castalia)", oem ? oem : doem);
        label(6, "OEM identity", buf, strong);
        label(7, "Boot profile", profile_name(prof), amber);
    } else {
        label(5, "Kernel", "FreeDOS-compatible (stock)", strong);
        sprintf(buf, "%02Xh", doem);
        label(6, "OEM identity", buf, strong);
        label(7, "Boot profile", "(kernel does not report)",
              UI_ATTR(C_DGRAY, C_LGRAY));   /* dim, but on the card */
    }

    sprintf(buf, "reports %d.%02d (MS-DOS compatible)", dmaj, dmin);
    label(8, "DOS version", buf, strong);

    ui_hline(CX0 + 1, CY0 + 9, CW - 2, A_PANEL);

    /* Machine. */
    label(10, "Processor", cpu, strong);
    label(11, "Coprocessor", fpu_present() ? "present" : "none", strong);

    {
        /* The XMS driver is the authority on extended memory: once it is
         * loaded the BIOS call answers 0 by design (see XMSINFO.C). */
        int   src = XMEM_NONE;
        unsigned ekb = mem_ext_kb(&src);
        if (src == XMEM_XMS)
            sprintf(buf, "%u KB base  -  %u KB XMS free", conv_kb(), ekb);
        else if (src == XMEM_BIOS)
            sprintf(buf, "%u KB base  -  %u KB ext.", conv_kb(), ekb);
        else
            sprintf(buf, "%u KB base  -  no extended memory", conv_kb());
        label(12, "Memory", buf, strong);
    }
    {
        unsigned xver = 0;
        if (xms_version(&xver) && xver != 0)
            sprintf(buf, "present (version %x.%02x)",
                    (xver >> 8) & 0xFF, xver & 0xFF);
        else
            strcpy(buf, xms_present() ? "present" : "not loaded");
        label(13, "XMS driver", buf, strong);
    }

    ui_hline(CX0 + 1, CY0 + 14, CW - 2, A_PANEL);

    /* Date (static) + a live clock updated until a key is pressed. */
    {
        int y, mo, d;
        get_date(&y, &mo, &d);
        sprintf(buf, "%04d-%02d-%02d", y, mo, d);
        label(15, "Date", buf, strong);
    }

    /* Not A_HINT: that is dim-on-blue for the desktop, and on this
     * light-gray card it painted a blue strip across the panel. */
    ui_puts(CX0 + 3, CY0 + CH - 1 - 1,
            "A Tombatossals Softworks product.",
            UI_ATTR(C_DGRAY, C_LGRAY));

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(1, SCR_H - 1,
            " CASTID - Castalia system signature      Press any key to leave",
            A_STATUS);
}

int main(void)
{
    char buf[64];
    unsigned char amber = UI_ATTR(C_BROWN, C_LGRAY);

    ui_init();
    paint_card();

    /* Live rows: the wall clock and the uptime, refreshed about twice a
     * second until a key lands.  T wakes the giant; anything else leaves. */
    for (;;) {
        int h, mi, s, k;
        unsigned long next;

        get_time(&h, &mi, &s);
        sprintf(buf, "%02d:%02d:%02d", h, mi, s);
        label(16, "Time", buf, amber);

        uptime_str(buf);
        label(17, "Uptime", buf, amber);

        next = ui_ticks() + 9UL;            /* ~0.5 s */
        while (ui_ticks() < next && !ui_keywaiting()) {
            if (ui_ticks() + 20UL < next)   /* midnight wrap guard */
                next = ui_ticks();
            ui_idle();
        }
        if (ui_keywaiting()) {
            k = ui_getkey();
            if (k == 't' || k == 'T') {
                wake_the_giant();
                paint_card();
                continue;
            }
            break;
        }
    }

    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
