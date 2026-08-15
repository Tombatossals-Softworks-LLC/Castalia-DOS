/* ===================================================================
 * BANNER.C  -  CASTALIA DOS animated boot banner  (BANNER.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * The face of the machine at boot.  The fortress keep rises from the
 * ground, the "CASTALIA DOS" wordmark lights up amber, the tower flags
 * flicker - then it gets out of the way.  Any key skips instantly and
 * the whole show is over in ~3 seconds, honouring the performance rules
 * (never block the boot).
 *
 *   BANNER            animated boot banner (installed system)
 *   BANNER /Q         instant, no animation (also used by slow terms)
 *   BANNER /R         rescue/installer disk splash + command list
 *   BANNER /R /Q      instant rescue splash
 *
 * The keep art and the block-letter wordmark live in the shared LOGO
 * module so the banner, the rescue disk, and the installer wear the
 * same face (see src/common/LOGO.C and docs/BRANDING.md).
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os banner.c ..\common\ui.c ..\common\logo.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/UI.H"
#include "../common/LOGO.H"

#define KEEP_X   ((SCR_W - LOGO_KEEP_W) / 2)     /* centered keep      */
#define WORD     "CASTALIA DOS"

/* A few stars in the Mediterranean night sky, kept clear of the centered
 * keep (cols ~18..60). */
struct star { int x, y; };
static const struct star stars[] = {
    { 4, 2 }, { 10, 5 }, { 6, 8 }, { 14, 3 }, { 3, 6 },
    { 75, 2 }, { 69, 5 }, { 73, 8 }, { 66, 3 }, { 77, 7 }
};
#define NSTARS ((int)(sizeof(stars) / sizeof(stars[0])))

static void draw_stars(unsigned char attr)
{
    int i;
    for (i = 0; i < NSTARS; i++)
        ui_putc(stars[i].x, stars[i].y, (char)0xFA, attr);   /* CP437 dot */
}

/* Wait n ticks; returns 1 (and eats the key) if a key was pressed. */
static int wait_ticks(int n)
{
    unsigned long until = ui_ticks() + (unsigned long)n;
    while (ui_ticks() < until) {
        if (ui_keywaiting()) {
            ui_getkey();
            return 1;
        }
        if (ui_ticks() + 100UL < until)     /* midnight wrap guard */
            return 0;
    }
    return 0;
}

/* Raise the keep from the ground up.  Returns 1 if the user skipped. */
static int raise_keep(int y0, int quick, int skipped)
{
    int r;
    if (quick) {
        logo_keep(KEEP_X, y0);
        return skipped;
    }
    for (r = LOGO_KEEP_H - 1; r >= 0; r--) {
        logo_keep_row(KEEP_X, y0, r);
        if (!skipped && wait_ticks(1))
            skipped = 1;
    }
    return skipped;
}

/* Reveal the wordmark dark -> steel -> amber.  Returns skip state. */
static int light_wordmark(int y, int quick, int skipped)
{
    if (quick) {
        logo_bigtext_center(y, WORD, UI_ATTR(C_YELLOW, C_BLUE));
        return skipped;
    }
    logo_bigtext_center(y, WORD, UI_ATTR(C_DGRAY, C_BLUE));
    if (!skipped && wait_ticks(2)) skipped = 1;
    logo_bigtext_center(y, WORD, UI_ATTR(C_LGRAY, C_BLUE));
    if (!skipped && wait_ticks(2)) skipped = 1;
    logo_bigtext_center(y, WORD, UI_ATTR(C_YELLOW, C_BLUE));
    return skipped;
}

/* While the view holds: pennants flicker amber/red and the lit windows
 * flicker like candlelight (amber/brown), so the keep quietly breathes. */
static void flicker_keep(int y0, int quick, int skipped)
{
    int i;
    if (quick)
        return;
    for (i = 0; i < 10 && !skipped; i++) {
        logo_keep_flags(KEEP_X, y0,
                        (i & 1) ? UI_ATTR(C_LRED, C_BLUE)
                                : UI_ATTR(C_YELLOW, C_BLUE));
        logo_keep_windows(KEEP_X, y0,
                        (i & 1) ? UI_ATTR(C_BROWN, C_BLUE)
                                : UI_ATTR(C_YELLOW, C_BLUE));
        draw_stars(UI_ATTR(C_DGRAY, C_BLUE));            /* all dim ... */
        ui_putc(stars[i % NSTARS].x, stars[i % NSTARS].y,
                (char)0xFA, UI_ATTR(C_WHITE, C_BLUE));   /* ... one twinkles */
        if (wait_ticks(3))
            skipped = 1;
    }
    logo_keep_flags(KEEP_X, y0, UI_ATTR(C_YELLOW, C_BLUE));
    logo_keep_windows(KEEP_X, y0, UI_ATTR(C_YELLOW, C_BLUE));
    draw_stars(UI_ATTR(C_DGRAY, C_BLUE));
}

/* --- the installed-system boot banner ------------------------------- */

static void banner_boot(int quick)
{
    int skipped = 0;
    char buf[64];
    const char *env = getenv("CASTPROFILE");

    skipped = raise_keep(0, quick, skipped);
    draw_stars(UI_ATTR(C_DGRAY, C_BLUE));
    skipped = light_wordmark(13, quick, skipped);

    ui_center(19, "386SX Edition   -   1.0  \"Tombatossals\"",
              UI_ATTR(C_WHITE, C_BLUE));
    ui_center(20, "a Tombatossals Softworks product",
              UI_ATTR(C_LGRAY, C_BLUE));
    /* The people who built it, credited quietly under the studio line -
     * the CP437 dot (0xFA) separates the names. */
    ui_center(21, "Dave Abellan  \xFA  Claudio di Castello", A_HINT);
    if (env != NULL && env[0] != '\0') {
        sprintf(buf, "profile: %s", env);
        ui_center(22, buf, A_HINT);
    }
    ui_center(23, "the fortress holds", A_HINT);

    flicker_keep(0, quick, skipped);

    /* Animated mode clears to a black field so the Castalia menu draws
     * clean.  Quick mode exists to show the SAME static banner without
     * animation (slow terminals), so it must leave the banner up - an
     * unconditional clear here would flash it to a blank screen. */
    if (!quick)
        ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
}

/* --- the rescue / installer disk splash ----------------------------- */

static void rescue_menu(void)
{
    int bx = 8, by = 17, bw = SCR_W - 16, bh = 7;   /* rows 17..23; 24 free */
    ui_fill(bx, by, bw, bh, ' ', A_DESKTOP);
    ui_box(bx, by, bw, bh, A_FRAME);
    ui_puts(bx + 2, by, " Rescue & Install Disk ", A_TITLE);
    ui_puts(bx + 3, by + 1,
            "SETUP     install CASTALIA DOS to the hard disk", A_ITEM);
    ui_puts(bx + 3, by + 2,
            "SAFEBOOT  repair an installed system's boot files", A_ITEM);
    ui_puts(bx + 3, by + 3,
            "HWINFO    inspect this machine", A_ITEM);
    ui_puts(bx + 3, by + 4,
            "FDISK  FORMAT  SYS  CHKDSK   live in A:\\DOS", A_HINT);
    ui_puts(bx + 3, by + 5,
            "Type a command and press Enter.", UI_ATTR(C_LCYAN, C_BLUE));
}

static void banner_rescue(int quick)
{
    int skipped = 0;

    skipped = raise_keep(0, quick, skipped);
    draw_stars(UI_ATTR(C_DGRAY, C_BLUE));
    skipped = light_wordmark(12, quick, skipped);
    rescue_menu();
    (void)skipped;

    /* Leave the splash on screen.  main() -> ui_done() drops the cursor to
     * the last row, so the shell prompt appears just below the command box.
     * We must NOT clear to black here: on a real floppy that produced a
     * multi-second black gap while AUTOEXEC slowly finished.  The rescue
     * AUTOEXEC has nothing to print after us, so the prompt follows at once. */
}

/* --- entry ---------------------------------------------------------- */

int main(int argc, char *argv[])
{
    int quick = 0, rescue = 0, i;

    for (i = 1; i < argc; i++) {
        char c;
        if (argv[i][0] != '/' && argv[i][0] != '-')
            continue;
        c = argv[i][1];
        if (c == 'q' || c == 'Q')
            quick = 1;
        else if (c == 'r' || c == 'R')
            rescue = 1;
    }

    ui_init();
    ui_cls(A_DESKTOP);

    if (rescue)
        banner_rescue(quick);
    else
        banner_boot(quick);

    ui_done();
    return 0;
}
