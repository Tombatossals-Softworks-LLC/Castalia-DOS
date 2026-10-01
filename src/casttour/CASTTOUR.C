/* ===================================================================
 * CASTTOUR.C  -  CASTALIA GUIDED TOUR  (CASTTOUR.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * An interactive, animated walk through Castalia DOS - and, left to
 * itself, an attract-mode demo.  Seven slides: the keep, the kernel
 * identity, the eight boot profiles, the tool suite, the minigames,
 * the minimum machine and the closing gate.  Every slide keeps moving
 * while it is on screen: reveals, typewriters, filling meters, sweeping
 * spotlights, candle-flicker, twinkles and shimmer.
 *
 * The user drives it: Right/Space/Enter walks forward, Left walks back,
 * Home returns to the first slide, A toggles the auto-tour, Esc leaves.
 * The BIOS key buffer is polled every turn of the loop, so a key never
 * waits on the animation.
 *
 * Built for a 386SX: one full repaint per slide change, everything
 * after that is incremental - only the cells that actually change get
 * rewritten.  The loop is paced off the BIOS tick and yields with
 * ui_idle() so it never spins a real CPU or an emulator core.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os casttour.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/UI.H"

/* --- tour geometry and pacing ---------------------------------------- */
#define NSLIDES     7
#define FRAME_TICKS 2UL         /* ~9 frames a second off the BIOS tick  */
#define AUTO_FRAMES 55U         /* ~6 seconds a slide in auto mode       */
#define PROG_Y      1           /* slim tour-position indicator          */
#define STATUS_Y    (SCR_H - 1)
#define PSEG_W      10          /* progress blocks per slide             */
#define PSEG_GAP    11

static const char *slide_title[NSLIDES] = {
    "The keep", "The kernel", "Boot profiles", "The suite",
    "The games", "Made for the metal", "The gate"
};

static int      cur;            /* slide on screen, 0..NSLIDES-1         */
static int      autorun;        /* attract mode on?                      */
static unsigned frame;          /* global animation frame counter        */
static unsigned slide_f0;       /* frame the current slide started at    */
static unsigned auto_f0;        /* frame the auto countdown started at   */

/* --- shared helpers --------------------------------------------------- */

/* Slide a three-cell bright band along a line of text, repainting only
 * the cells it just left and the cells it now covers. */
static void shim_line(const char *s, int x, int y, int oldp, int newp,
                      unsigned char base, unsigned char mid, unsigned char hi)
{
    int n = (int)strlen(s), i, d;
    for (i = oldp - 2; i <= oldp + 2; i++)
        if (i >= 0 && i < n) ui_putc(x + i, y, s[i], base);
    for (i = newp - 2; i <= newp + 2; i++)
        if (i >= 0 && i < n) {
            d = i - newp;
            if (d < 0) d = -d;
            ui_putc(x + i, y, s[i], (d == 0) ? hi : (d == 1) ? mid : base);
        }
}

/* A tiny shared starfield, reused by the keep and the gate. */
#define NSTAR 24
static int           star_x[NSTAR], star_y[NSTAR], nstar;
static char          star_ch[NSTAR];
static unsigned char star_at[NSTAR];

static void star_roll(int i)
{
    int r = rand() % 100;
    if (r < 45)      { star_ch[i] = (char)0xFA; star_at[i] = UI_ATTR(C_DGRAY, C_BLUE); }
    else if (r < 75) { star_ch[i] = (char)0xF9; star_at[i] = UI_ATTR(C_LGRAY, C_BLUE); }
    else if (r < 90) { star_ch[i] = (char)'*';  star_at[i] = UI_ATTR(C_WHITE, C_BLUE); }
    else             { star_ch[i] = (char)0x0F; star_at[i] = UI_ATTR(C_YELLOW, C_BLUE); }
}

static void stars_place(const int *rows, int nrows, int count)
{
    int i;
    nstar = (count > NSTAR) ? NSTAR : count;
    for (i = 0; i < nstar; i++) {
        star_x[i] = rand() % SCR_W;
        star_y[i] = rows[rand() % nrows];
        star_roll(i);
        ui_putc(star_x[i], star_y[i], star_ch[i], star_at[i]);
    }
}

static void stars_twinkle(int n)
{
    int k, i;
    for (k = 0; k < n && nstar > 0; k++) {
        i = rand() % nstar;
        star_roll(i);
        ui_putc(star_x[i], star_y[i], star_ch[i], star_at[i]);
    }
}

/* Marker plus a padded, highlightable name at the head of a list row. */
static void row_head(int mx, int nx, int y, const char *name, int hot)
{
    ui_putc(mx, y, hot ? (char)0x10 : ' ', UI_ATTR(C_WHITE, C_BLUE));
    ui_puts(nx, y, name, hot ? UI_ATTR(C_WHITE, C_BLUE)
                             : UI_ATTR(C_YELLOW, C_BLUE));
}

/* --- persistent chrome: title bar, tour progress, status bar ---------- */

static void draw_titlebar(void)
{
    int n = (int)strlen(slide_title[cur]);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA DOS", UI_ATTR(C_WHITE, C_BLUE));
    ui_puts(15, 0, "- the guided tour", A_TITLE);
    ui_putc(SCR_W - 4 - n, 0, (char)0x10, A_TITLE);
    ui_puts(SCR_W - 2 - n, 0, slide_title[cur], UI_ATTR(C_WHITE, C_BLUE));
}

static void draw_progress(void)
{
    int i, j, x;
    for (i = 0; i < NSLIDES; i++) {
        x = 2 + i * PSEG_GAP;
        for (j = 0; j < PSEG_W; j++) {
            if (i < cur)       ui_putc(x + j, PROG_Y, (char)0xDB, UI_ATTR(C_BROWN, C_BLUE));
            else if (i > cur)  ui_putc(x + j, PROG_Y, (char)0xC4, UI_ATTR(C_DGRAY, C_BLUE));
            else               ui_putc(x + j, PROG_Y, (char)0xB1, UI_ATTR(C_BROWN, C_BLUE));
        }
    }
}

/* The segment for the slide you are on breathes a bright cell along it. */
static void step_progress(unsigned sf)
{
    int x = 2 + cur * PSEG_GAP, p = (int)(sf % (unsigned)(PSEG_W + 4)), j, d;
    char ch;
    unsigned char a;
    for (j = 0; j < PSEG_W; j++) {
        d = j - p;
        if (d < 0) d = -d;
        if (d == 0)      { ch = (char)0xDB; a = UI_ATTR(C_WHITE,  C_BLUE); }
        else if (d == 1) { ch = (char)0xB2; a = UI_ATTR(C_YELLOW, C_BLUE); }
        else             { ch = (char)0xB1; a = UI_ATTR(C_BROWN,  C_BLUE); }
        ui_putc(x + j, PROG_Y, ch, a);
    }
}

/* AUTO / MAN plus a five-cell countdown to the next auto advance. */
static void step_auto_meter(void)
{
    unsigned el = (unsigned)(frame - auto_f0);
    int pm = 0;
    ui_puts(69, STATUS_Y, autorun ? "AUTO" : "MAN ",
            autorun ? UI_ATTR(C_RED, C_LGRAY) : UI_ATTR(C_DGRAY, C_LGRAY));
    if (autorun) {
        if (el > AUTO_FRAMES) el = AUTO_FRAMES;
        pm = (int)(((long)el * 1000L) / (long)AUTO_FRAMES);
    }
    ui_hbar(74, STATUS_Y, 5, pm, UI_ATTR(C_BLUE, C_LGRAY),
            UI_ATTR(C_DGRAY, C_LGRAY));
}

static void draw_status(void)
{
    char buf[24];
    ui_fill(0, STATUS_Y, SCR_W, 1, ' ', A_STATUS);
    ui_puts(1, STATUS_Y,
            "Right/Space next  Left back  Home first  A auto  Esc quit",
            A_STATUS);
    sprintf(buf, "Slide %d/%d", cur + 1, NSLIDES);
    ui_puts(59, STATUS_Y, buf, UI_ATTR(C_BLUE, C_LGRAY));
    step_auto_meter();
}

/* --- slide 1: the keep ------------------------------------------------ */
#define KEEP_BASE 22
#define NKWIN     24
#define PEN_X     39
#define PEN_Y     10

static int           keep_top[SCR_W], kwin_x[NKWIN], kwin_y[NKWIN], nkwin;
static unsigned char kwin_on[NKWIN];
static int           keep_col;      /* columns of silhouette revealed    */
static int           keep_pen;      /* pennant flutter phase             */
static int           keep_shim, keep_tagx;

static const char *KEEP_TAG =
    "CASTALIA DOS - built for real machines, not screenshots.";
static const int keep_rows[2] = { 8, 9 };

static void build_keep(void)
{
    static const int tx0[5] = {  4, 20, 34, 50, 64 };
    static const int tx1[5] = { 12, 28, 44, 58, 72 };
    static const int tt[5]  = { 15, 14, 11, 14, 15 };
    int c, t, ri, ci, nc, rows[2], cols[3];

    for (c = 0; c < SCR_W; c++) keep_top[c] = 18;       /* curtain wall  */
    for (t = 0; t < 5; t++)
        for (c = tx0[t]; c <= tx1[t]; c++)
            if (tt[t] < keep_top[c]) keep_top[c] = tt[t];
    for (c = 0; c < SCR_W; c++)
        if (c & 1) keep_top[c] += 1;                    /* merlons       */

    nkwin = 0;
    for (t = 0; t < 5; t++) {
        rows[0] = tt[t] + 2; rows[1] = tt[t] + 4;
        cols[0] = tx0[t] + 2; cols[1] = tx1[t] - 2; nc = 2;
        if (t == 2) { cols[2] = (tx0[t] + tx1[t]) / 2; nc = 3; }
        for (ri = 0; ri < 2; ri++)
            for (ci = 0; ci < nc; ci++)
                if (nkwin < NKWIN) {
                    kwin_x[nkwin] = cols[ci];
                    kwin_y[nkwin] = rows[ri];
                    kwin_on[nkwin] = 2;
                    nkwin++;
                }
    }
}

static void draw_kwin(int i)
{
    unsigned char a;
    if (kwin_on[i] == 2)      a = UI_ATTR(C_YELLOW, C_BLUE);
    else if (kwin_on[i] == 1) a = UI_ATTR(C_BROWN,  C_BLUE);
    else                      a = UI_ATTR(C_BLACK,  C_BLUE);
    ui_putc(kwin_x[i], kwin_y[i], (char)0xDB, a);
}

static void draw_pennant(void)
{
    ui_putc(PEN_X + 1, PEN_Y, (char)0x1E,
            keep_pen ? UI_ATTR(C_YELLOW, C_BLUE) : UI_ATTR(C_LRED, C_BLUE));
    ui_putc(PEN_X + 2, PEN_Y, keep_pen ? (char)0xFA : ' ',
            UI_ATTR(C_BROWN, C_BLUE));
}

static void enter_keep(void)
{
    keep_col = 0; keep_pen = 0; keep_shim = -8;
    keep_tagx = (SCR_W - (int)strlen(KEEP_TAG)) / 2;
    ui_puts(keep_tagx, 4, KEEP_TAG, UI_ATTR(C_YELLOW, C_BLUE));
    ui_center(6, "Right and Left walk the walls.  A starts the attract tour.",
              A_HINT);
    stars_place(keep_rows, 2, 16);
}

static void step_keep(unsigned sf)
{
    int c, r, i, n, old;

    if (keep_col < SCR_W) {                     /* the keep rises        */
        for (n = 0; n < 3 && keep_col < SCR_W; n++) {
            c = keep_col;
            for (r = keep_top[c]; r <= KEEP_BASE; r++)
                ui_putc(c, r, (char)0xDB, UI_ATTR(C_DGRAY, C_BLUE));
            for (i = 0; i < nkwin; i++)
                if (kwin_x[i] == c) draw_kwin(i);
            if (c == PEN_X)     ui_putc(PEN_X, PEN_Y, (char)0xB3, UI_ATTR(C_LGRAY, C_BLUE));
            if (c == PEN_X + 2) draw_pennant();
            keep_col++;
        }
        return;
    }
    if ((sf & 1U) == 0U)                        /* candlelight flickers  */
        for (n = 0; n < 2; n++) {
            i = rand() % nkwin;
            r = rand() % 100;
            kwin_on[i] = (unsigned char)((r < 70) ? 2 : (r < 92) ? 1 : 0);
            draw_kwin(i);
        }
    if ((sf % 5U) == 0U) {                      /* the pennant flutters  */
        keep_pen = !keep_pen;
        draw_pennant();
    }
    stars_twinkle(2);
    old = keep_shim;                            /* tagline shimmer       */
    if (++keep_shim > (int)strlen(KEEP_TAG) + 8) keep_shim = -8;
    shim_line(KEEP_TAG, keep_tagx, 4, old, keep_shim,
              UI_ATTR(C_BROWN, C_BLUE), UI_ATTR(C_YELLOW, C_BLUE),
              UI_ATTR(C_WHITE, C_BLUE));
}

/* --- slide 2: the kernel identity, typed out line by line -------------
 * The slide shows where each answer lives (docs/KERNEL.md), not values:
 * the tour never makes the call, and made-up numbers on a slide about a
 * kernel that "never pretends" would be the one thing it must not do.
 * CASTID is the tool that reads them live. */
#define KLINES   13
#define KX       10
#define KY       6
#define TYPE_CPS 12             /* characters per animation frame        */

static const char *kern_line[KLINES] = {
    "CASTALIA kernel identity  -  INT 2Fh  AH=CAh",
    "",
    "  Build number    CA00h  CX",
    "  Edition         CA00h  DH   (01h = 386SX Edition)",
    "  OEM identity    CA00h  DL   (CAh = Castalia)",
    "  Boot profile    CA01h  CL   (the one that booted)",
    "  Behaviour       MS-DOS 6.22 compatible",
    "",
    "  Castalia is built on the FreeDOS kernel (GPLv2+)",
    "  and says so.  Castalia code and branding: MIT.",
    "",
    "  On a stock kernel the identity call is ignored",
    "  and the tools say so - Castalia never pretends."
};

static int ty_l, ty_c;          /* next character to type                */
static int ty_cx, ty_cy;        /* where the caret currently sits        */
static int ty_blink, kshim;

static unsigned char kern_attr(int l)
{
    if (l == 0)           return UI_ATTR(C_BLUE,  C_LGRAY);
    if (l == 8 || l == 9) return UI_ATTR(C_RED,   C_LGRAY);
    if (l >= 11)          return UI_ATTR(C_DGRAY, C_LGRAY);
    return A_PANEL;
}

static void enter_kernel(void)
{
    ty_l = 0; ty_c = 0; ty_cx = -1; ty_cy = 0; ty_blink = 0; kshim = -6;
    ui_fill(8, 4, 64, 18, ' ', A_PANEL);
    ui_box(8, 4, 64, 18, A_PANEL);
    ui_fill(9, 4, 62, 1, ' ', A_PANELHDR);
    ui_puts(11, 4, " The kernel says who it is ", A_PANELHDR);
}

static void step_kernel(unsigned sf)
{
    int n, len, old;

    if (ty_l < KLINES) {                        /* the typewriter runs   */
        if (ty_cx >= 0) ui_putc(ty_cx, ty_cy, ' ', A_PANEL);
        for (n = 0; n < TYPE_CPS && ty_l < KLINES; ) {
            len = (int)strlen(kern_line[ty_l]);
            if (ty_c >= len) { ty_l++; ty_c = 0; continue; }
            ui_putc(KX + ty_c, KY + ty_l, kern_line[ty_l][ty_c], kern_attr(ty_l));
            ty_c++; n++;
        }
        if (ty_l < KLINES) {
            ty_cx = KX + ty_c; ty_cy = KY + ty_l;
            ui_putc(ty_cx, ty_cy, (char)0xDB, UI_ATTR(C_BLUE, C_LGRAY));
        } else {
            ty_cx = -1;
        }
        return;
    }
    if ((sf % 4U) == 0U) {                      /* a caret waits at C:\> */
        ty_blink = !ty_blink;
        ui_puts(KX, KY + KLINES + 1, "C:\\>", UI_ATTR(C_BLUE, C_LGRAY));
        ui_putc(KX + 5, KY + KLINES + 1, ty_blink ? (char)0xDB : ' ',
                UI_ATTR(C_BLUE, C_LGRAY));
    }
    old = kshim;                                /* header shimmer        */
    if (++kshim > (int)strlen(kern_line[0]) + 6) kshim = -6;
    shim_line(kern_line[0], KX, KY, old, kshim, UI_ATTR(C_BLUE, C_LGRAY),
              UI_ATTR(C_BLACK, C_LGRAY), UI_ATTR(C_WHITE, C_LGRAY));
}

/* --- slide 3: the eight boot profiles, as filling memory meters ------- */
#define NPROF   8
#define PBAR_X  13
#define PBAR_W  30

static const char *prof_name[NPROF] = {
    "CLEAN   ", "XMS     ", "EMS     ", "CDROM   ",
    "WIN3X   ", "DIAG    ", "SAFE    ", "PROMPT  "
};
static const int prof_kb[NPROF] = { 615, 628, 600, 585, 590, 0, 635, 615 };
static const char *prof_note[NPROF] = {
    "nothing loaded but DOS",   "HIMEM + JEMM386 NOEMS",
    "EMS page frame for games", "CD-ROM driver + MSCDEX",
    "tuned for Windows 3.x",    "diagnostic shell - varies",
    "recovery, bare minimum",   "straight to the prompt"
};
static int prof_sel, prof_scan;

/* 640 KB full scale in permille, kept inside a 16-bit int: kb*25/16. */
static int prof_pm(int i) { return prof_kb[i] * 25 / 16; }

static void draw_prof_row(int i, int hot, int pm)
{
    int y = 5 + i * 2;
    char v[16];
    row_head(2, 4, y, prof_name[i], hot);
    if (prof_kb[i] > 0) {
        ui_hbar(PBAR_X, y, PBAR_W, pm,
                hot ? UI_ATTR(C_YELLOW, C_BLUE) : UI_ATTR(C_LGREEN, C_BLUE),
                UI_ATTR(C_DGRAY, C_BLUE));
        sprintf(v, "%3d KB", prof_kb[i]);
    } else {
        ui_fill(PBAR_X, y, PBAR_W, 1, (char)0xFA, UI_ATTR(C_DGRAY, C_BLUE));
        strcpy(v, "   -  ");
    }
    ui_puts(44, y, v, hot ? UI_ATTR(C_WHITE, C_BLUE) : A_ITEM);
    ui_puts(52, y, prof_note[i], hot ? UI_ATTR(C_LCYAN, C_BLUE) : A_HINT);
}

static void enter_profiles(void)
{
    prof_sel = -1; prof_scan = 0;
    ui_center(3, "Eight boot profiles - conventional memory left for you",
              UI_ATTR(C_YELLOW, C_BLUE));
    ui_center(21, "Choose one at the boot menu, or let SAFEBOOT choose it.",
              A_HINT);
}

static void step_profiles(unsigned sf)
{
    int i, sel, y = 5 + 5 * 2;
    unsigned base, d;

    for (i = 0; i < NPROF; i++) {               /* meters fill in turn   */
        base = (unsigned)(i * 3);
        if (sf < base) continue;
        d = sf - base;
        if (d <= 8U) draw_prof_row(i, 0, prof_pm(i) * (int)d / 8);
    }
    if (sf > 15U) {                             /* DIAG keeps scanning   */
        ui_putc(PBAR_X + prof_scan, y, (char)0xFA, UI_ATTR(C_DGRAY, C_BLUE));
        prof_scan = (prof_scan + 1) % PBAR_W;
        ui_putc(PBAR_X + prof_scan, y, (char)0xDB, UI_ATTR(C_LCYAN, C_BLUE));
    }
    if (sf >= 34U) {                            /* spotlight walks down  */
        sel = (int)((sf - 34U) / 6U % (unsigned)NPROF);
        if (sel != prof_sel) {
            if (prof_sel >= 0) draw_prof_row(prof_sel, 0, prof_pm(prof_sel));
            prof_sel = sel;
            draw_prof_row(sel, 1, prof_pm(sel));
        }
    }
}

/* --- slide 4: the tool suite, cascading in ---------------------------- */
#define NTOOL 17

static const char *tool_name[NTOOL] = {
    "CASTALIA ", "LAUNCH   ", "HWINFO   ", "CASTMARK ", "CASTFM   ",
    "CASTCOPY ", "CASTDOC  ", "MEMPROF  ", "SETSOUND ", "CFGEDIT  ",
    "CASTEDIT ", "GAMECFG  ", "HELP     ", "SAFEBOOT ", "CASTID   ",
    "CDPLAYER ", "SAVER    "
};
static const char *tool_desc[NTOOL] = {
    "the shell, the menu and the launcher",
    "run a program with the right memory profile",
    "hardware detection and diagnostics",
    "CPU, memory, disk and video benchmark",
    "two-pane file manager",
    "verified file copy with a progress bar",
    "the Disk Doctor: read-only surface scan",
    "conventional memory map and profiles",
    "sound card setup and the BLASTER string",
    "CONFIG.SYS and AUTOEXEC.BAT editor",
    "a plain text editor that fits in 640 KB",
    "the per-game configuration database",
    "the help browser",
    "recovery boot and repair",
    "the system signature card",
    "CD audio player",
    "CASTALIA NIGHT, the screensaver"
};
static int tool_sel;

static void draw_tool_row(int i, int hot)
{
    row_head(3, 5, 5 + i, tool_name[i], hot);
    ui_puts(16, 5 + i, tool_desc[i], hot ? UI_ATTR(C_LCYAN, C_BLUE) : A_ITEM);
}

static void enter_tools(void)
{
    tool_sel = -1;
    ui_center(3, "Seventeen tools ship in the box - all of them keyboard-first",
              UI_ATTR(C_YELLOW, C_BLUE));
}

static void step_tools(unsigned sf)
{
    int i, sel;
    if (sf < 40U) {                             /* staggered cascade     */
        for (i = 0; i < NTOOL; i++) {
            if (sf == (unsigned)(i * 2))          draw_tool_row(i, 1);
            else if (sf == (unsigned)(i * 2 + 3)) draw_tool_row(i, 0);
        }
        return;
    }
    sel = (int)((sf - 40U) / 2U % (unsigned)NTOOL);   /* spotlight sweep */
    if (sel != tool_sel) {
        if (tool_sel >= 0) draw_tool_row(tool_sel, 0);
        tool_sel = sel;
        draw_tool_row(sel, 1);
    }
}

/* --- slide 5: the minigames, with three live vignettes ---------------- */
#define NGAME   8
#define GND_Y   20              /* shared ground line in the vignettes   */
#define SNK_X0  4
#define SNK_X1  32
#define SNK_Y   19
#define SNK_LEN 6
#define WELL_L  35              /* Almena well: walls at 35 and 42       */
#define WELL_R  42
#define SIE_X0  46
#define SIE_T   28

static const char *game_name[NGAME] = {
    "Snake     ", "15-Puzzle ", "Almena    ", "Minas     ",
    "Siege     ", "Reversi   ", "Barrels   ", "Solitaire "
};
static const char *game_desc[NGAME] = {
    "the serpent loose in the bailey",
    "fifteen tiles and one empty square",
    "falling stones - keep the wall whole",
    "minesweeper along the castle moat",
    "a catapult duel across the skyline",
    "8x8 othello against an alpha-beta foe",
    "sokoban down in the fortress cellar",
    "Klondike, draw one, on green baize"
};

static int snk_x, snk_apple;                    /* snake vignette        */
static int alm_x, alm_y, alm_fill, alm_flash;   /* Almena vignette       */
static int sie_t, sie_x, sie_y;                 /* Siege vignette        */

static void enter_games(void)
{
    int i;
    ui_center(3, "Eight minigames - text mode, keyboard only, no excuses",
              UI_ATTR(C_YELLOW, C_BLUE));
    for (i = 0; i < NGAME; i++) {
        ui_putc(4, 5 + i, (char)0x07, UI_ATTR(C_BROWN, C_BLUE));
        ui_puts(6, 5 + i, game_name[i], UI_ATTR(C_YELLOW, C_BLUE));
        ui_puts(18, 5 + i, game_desc[i], A_ITEM);
    }
    ui_box(2, 14, 76, 8, A_FRAME);
    ui_puts(5, 14, " live from the cartridge ", UI_ATTR(C_LCYAN, C_BLUE));
    ui_puts(SNK_X0, 15, "SNAKE", A_HINT);
    ui_puts(WELL_L, 15, "ALMENA", A_HINT);
    ui_puts(SIE_X0 + 1, 15, "SIEGE", A_HINT);
    ui_fill(3, GND_Y, 74, 1, (char)0xC4, UI_ATTR(C_DGRAY, C_BLUE));
    for (i = 16; i <= 19; i++) {
        ui_putc(WELL_L, i, (char)0xB3, UI_ATTR(C_LGRAY, C_BLUE));
        ui_putc(WELL_R, i, (char)0xB3, UI_ATTR(C_LGRAY, C_BLUE));
    }
    snk_x = SNK_X0; snk_apple = SNK_X0 + 10;
    ui_putc(snk_apple, SNK_Y, (char)0x04, UI_ATTR(C_YELLOW, C_BLUE));
    alm_fill = 0; alm_flash = 0; alm_x = WELL_L + 1; alm_y = 16;
    sie_t = 0; sie_x = SIE_X0; sie_y = 19;
}

static void step_snake(void)
{
    int tail = snk_x - SNK_LEN;
    if (snk_x >= SNK_X1 + SNK_LEN) {            /* lap done, reset strip */
        ui_fill(SNK_X0, SNK_Y, SNK_X1 - SNK_X0 + 1, 1, ' ', A_DESKTOP);
        snk_x = SNK_X0;
        snk_apple = SNK_X0 + 8 + rand() % 10;
        ui_putc(snk_apple, SNK_Y, (char)0x04, UI_ATTR(C_YELLOW, C_BLUE));
        return;
    }
    if (tail >= SNK_X0 && tail <= SNK_X1)
        ui_putc(tail, SNK_Y, ' ', A_DESKTOP);
    if (snk_x - 1 >= SNK_X0 && snk_x - 1 <= SNK_X1)
        ui_putc(snk_x - 1, SNK_Y, (char)0xDB, UI_ATTR(C_GREEN, C_BLUE));
    if (snk_x <= SNK_X1)
        ui_putc(snk_x, SNK_Y, (char)0xDB, UI_ATTR(C_LGREEN, C_BLUE));
    if (snk_x == snk_apple) {                   /* an apple goes down    */
        snk_apple = snk_x + 6 + rand() % 8;
        if (snk_apple <= SNK_X1)
            ui_putc(snk_apple, SNK_Y, (char)0x04, UI_ATTR(C_YELLOW, C_BLUE));
    }
    snk_x++;
}

static void step_almena(unsigned sf)
{
    int i;
    if (alm_flash > 0) {                        /* a full row flashes    */
        alm_flash--;
        for (i = WELL_L + 1; i < WELL_R; i++)
            ui_putc(i, 19, (char)0xDB, (alm_flash & 1)
                    ? UI_ATTR(C_WHITE, C_BLUE) : UI_ATTR(C_YELLOW, C_BLUE));
        if (alm_flash == 0) {
            ui_fill(WELL_L + 1, 19, WELL_R - WELL_L - 1, 1, ' ', A_DESKTOP);
            alm_fill = 0; alm_x = WELL_L + 1; alm_y = 16;
        }
        return;
    }
    if ((sf % 3U) != 0U) return;
    ui_putc(alm_x, alm_y, ' ', A_DESKTOP);      /* the stone falls       */
    alm_y++;
    if (alm_y < 19) {
        ui_putc(alm_x, alm_y, (char)0xDB, UI_ATTR(C_LMAGENTA, C_BLUE));
    } else {
        ui_putc(alm_x, 19, (char)0xDB, UI_ATTR(C_LRED, C_BLUE));
        alm_fill++;
        if (alm_fill >= WELL_R - WELL_L - 1) alm_flash = 6;
        else { alm_x = WELL_L + 1 + alm_fill; alm_y = 16; }
    }
}

static void step_siege(void)
{
    int h;
    ui_putc(sie_x, sie_y, ' ', A_DESKTOP);      /* erase the boulder     */
    if (++sie_t > SIE_T) {
        sie_t = 0;
        ui_putc(SIE_X0 + SIE_T, GND_Y, (char)0xC4, UI_ATTR(C_DGRAY, C_BLUE));
    }
    h = sie_t * (SIE_T - sie_t) / 65;           /* a flat parabola       */
    sie_x = SIE_X0 + sie_t;
    sie_y = 19 - h;
    ui_putc(sie_x, sie_y, (char)0x07, UI_ATTR(C_WHITE, C_BLUE));
    if (sie_t == SIE_T)                         /* it lands with a puff  */
        ui_putc(sie_x, GND_Y, (char)0xB0, UI_ATTR(C_LGRAY, C_BLUE));
}

static void step_games(unsigned sf)
{
    step_snake();
    step_almena(sf);
    if ((sf & 1U) == 0U) step_siege();
}

/* --- slide 6: made for the metal -------------------------------------- */
#define CARD_X 14
#define CARD_Y 3
#define CARD_W 52
#define CARD_H 18
#define CARD_P (2 * (CARD_W - 1) + 2 * (CARD_H - 1))
#define NSPEC  8

static const char *CARD_HDR = " MINIMUM MACHINE ";
static const char *spec_label[NSPEC] = {
    "Processor", "Memory", "Video",  "Storage",
    "Sound",     "Input",  "Kernel", "Media"
};
static const char *spec_val[NSPEC] = {
    "80386SX at 16 MHz, or better",
    "4 MB (2 MB is comfortable)",
    "VGA 80x25 text; CGA/EGA tolerated",
    "FAT12 / FAT16, 8.3 names",
    "PC speaker or Sound Blaster",
    "AT keyboard; a mouse is optional",
    "FreeDOS-based, DOS 6.22 behaviour",
    "boots from one 1.44 MB floppy"
};
static const char *METAL_TAG = "Compatibility beats elegance.  Every time.";
static int metal_sel, metal_run, metal_shim, metal_tagx;

/* Where perimeter step p sits on the card frame, and what belongs there.
 * attr 0 means "put back whatever was there before the runner lit it". */
static void card_cell(int p, unsigned char attr)
{
    int top = CARD_W - 1, right = top + CARD_H - 1, bottom = right + CARD_W - 1;
    int hx = CARD_X + 3, hn = (int)strlen(CARD_HDR), x, y;
    char ch;

    p = ((p % CARD_P) + CARD_P) % CARD_P;
    if (p < top)         { x = CARD_X + p;            y = CARD_Y; }
    else if (p < right)  { x = CARD_X + CARD_W - 1;   y = CARD_Y + (p - top); }
    else if (p < bottom) { x = CARD_X + CARD_W - 1 - (p - right);
                           y = CARD_Y + CARD_H - 1; }
    else                 { x = CARD_X;
                           y = CARD_Y + CARD_H - 1 - (p - bottom); }

    if (y == CARD_Y && x >= hx && x < hx + hn) {
        ch = CARD_HDR[x - hx];
        if (!attr) attr = UI_ATTR(C_YELLOW, C_BLUE);
    } else {
        if (x == CARD_X)                    ch = (y == CARD_Y) ? (char)0xC9
                                               : (y == CARD_Y + CARD_H - 1)
                                                 ? (char)0xC8 : (char)0xBA;
        else if (x == CARD_X + CARD_W - 1)  ch = (y == CARD_Y) ? (char)0xBB
                                               : (y == CARD_Y + CARD_H - 1)
                                                 ? (char)0xBC : (char)0xBA;
        else                                ch = (char)0xCD;
        if (!attr) attr = A_FRAME;
    }
    ui_putc(x, y, ch, attr);
}

static void draw_spec_row(int i, int hot)
{
    int y = CARD_Y + 2 + i * 2;
    row_head(CARD_X + 2, CARD_X + 4, y, spec_label[i], hot);
    ui_putlim(CARD_X + 17, y, spec_val[i], 34,
              hot ? UI_ATTR(C_LCYAN, C_BLUE) : A_ITEM);
}

static void enter_metal(void)
{
    int i;
    metal_sel = -1; metal_run = 0; metal_shim = -8;
    metal_tagx = (SCR_W - (int)strlen(METAL_TAG)) / 2;
    ui_dbox(CARD_X, CARD_Y, CARD_W, CARD_H, A_FRAME);
    ui_puts(CARD_X + 3, CARD_Y, CARD_HDR, UI_ATTR(C_YELLOW, C_BLUE));
    for (i = 0; i < NSPEC; i++) draw_spec_row(i, 0);
    ui_puts(metal_tagx, 22, METAL_TAG, UI_ATTR(C_BROWN, C_BLUE));
}

static void step_metal(unsigned sf)
{
    int sel, old;
    card_cell(metal_run - 2, 0);                /* runner rounds the card */
    card_cell(metal_run - 1, 0);
    card_cell(metal_run,     UI_ATTR(C_WHITE,  C_BLUE));
    card_cell(metal_run + 1, UI_ATTR(C_YELLOW, C_BLUE));
    card_cell(metal_run + 2, UI_ATTR(C_LGRAY,  C_BLUE));
    metal_run = (metal_run + 2) % CARD_P;

    sel = (int)(sf / 3U % (unsigned)NSPEC);     /* scanning highlight     */
    if (sel != metal_sel) {
        if (metal_sel >= 0) draw_spec_row(metal_sel, 0);
        metal_sel = sel;
        draw_spec_row(sel, 1);
    }
    old = metal_shim;
    if (++metal_shim > (int)strlen(METAL_TAG) + 8) metal_shim = -8;
    shim_line(METAL_TAG, metal_tagx, 22, old, metal_shim,
              UI_ATTR(C_BROWN, C_BLUE), UI_ATTR(C_YELLOW, C_BLUE),
              UI_ATTR(C_WHITE, C_BLUE));
}

/* --- slide 7: the gate ------------------------------------------------ */
#define MARK_W 60
#define MARK_Y 6

static const char big_set[] = "CASTLIDO ";
static const char *big_rows[9][5] = {
    { "####", "#   ", "#   ", "#   ", "####" },      /* C */
    { " ## ", "#  #", "####", "#  #", "#  #" },      /* A */
    { "####", "#   ", "####", "   #", "####" },      /* S */
    { "####", "  # ", "  # ", "  # ", "  # " },      /* T */
    { "#   ", "#   ", "#   ", "#   ", "####" },      /* L */
    { "####", "  # ", "  # ", "  # ", "####" },      /* I */
    { "### ", "#  #", "#  #", "#  #", "### " },      /* D */
    { " ## ", "#  #", "#  #", "#  #", " ## " },      /* O */
    { "    ", "    ", "    ", "    ", "    " }       /* space */
};

static unsigned char mgrid[5][MARK_W];
static int mark_w, mark_x, mark_sweep, gate_pulse;
static const char *GATE_EXIT = "Press Esc to leave the tour.";
static const int gate_rows[6] = { 3, 4, 5, 12, 21, 22 };

static void build_mark(void)
{
    static const char word[] = "CASTALIA DOS";
    const char *p;
    int i, r, c, gi, col = 0, n = (int)strlen(word);

    memset(mgrid, 0, sizeof(mgrid));
    for (i = 0; i < n; i++) {
        p = strchr(big_set, word[i]);
        gi = p ? (int)(p - big_set) : 8;
        for (r = 0; r < 5; r++)
            for (c = 0; c < 4; c++)
                if (col + c < MARK_W && big_rows[gi][r][c] == '#')
                    mgrid[r][col + c] = 1;
        col += 5;
    }
    mark_w = (col - 1 > MARK_W) ? MARK_W : col - 1;
    mark_x = (SCR_W - mark_w) / 2;
}

/* Repaint one wordmark column, shaded by its distance from the sweep. */
static void mark_col(int c)
{
    int r, d;
    unsigned char a;
    if (c < 0 || c >= mark_w) return;
    for (r = 0; r < 5; r++) {
        if (!mgrid[r][c]) continue;
        d = c + r - mark_sweep;
        if (d < 0) d = -d;
        if (d <= 1)      a = UI_ATTR(C_WHITE,  C_BLUE);
        else if (d <= 3) a = UI_ATTR(C_YELLOW, C_BLUE);
        else             a = UI_ATTR(C_BROWN,  C_BLUE);
        ui_putc(mark_x + c, MARK_Y + r, (char)0xDB, a);
    }
}

static void enter_gate(void)
{
    int c;
    mark_sweep = -8; gate_pulse = 0;
    for (c = 0; c < mark_w; c++) mark_col(c);
    ui_center(13, "A Tombatossals Softworks product.", UI_ATTR(C_YELLOW, C_BLUE));
    ui_center(15, "Built on FreeDOS (GPLv2+).  Castalia code and branding: MIT.",
              A_HINT);
    ui_center(17, "Right and Left walk the tour.  A runs it by itself.",
              UI_ATTR(C_LGRAY, C_BLUE));
    ui_center(19, GATE_EXIT, UI_ATTR(C_WHITE, C_BLUE));
    stars_place(gate_rows, 6, 22);
}

static void step_gate(unsigned sf)
{
    int c, lo = mark_sweep - 8, hi;
    if (++mark_sweep > mark_w + 10) {           /* new lap: full reset   */
        mark_sweep = -8;
        for (c = 0; c < mark_w; c++) mark_col(c);
    } else {
        hi = mark_sweep + 3;
        if (lo < 0) lo = 0;
        if (hi >= mark_w) hi = mark_w - 1;
        for (c = lo; c <= hi; c++) mark_col(c);
    }
    stars_twinkle(3);
    if ((sf % 4U) == 0U) {                      /* the exit line pulses  */
        gate_pulse = !gate_pulse;
        ui_center(19, GATE_EXIT, gate_pulse ? UI_ATTR(C_WHITE,  C_BLUE)
                                            : UI_ATTR(C_YELLOW, C_BLUE));
    }
}

/* --- slide plumbing --------------------------------------------------- */

static void slide_enter(void)
{
    nstar = 0;
    ui_cls(A_DESKTOP);
    draw_titlebar();
    draw_progress();
    draw_status();
    switch (cur) {
    case 0:  enter_keep();     break;
    case 1:  enter_kernel();   break;
    case 2:  enter_profiles(); break;
    case 3:  enter_tools();    break;
    case 4:  enter_games();    break;
    case 5:  enter_metal();    break;
    default: enter_gate();     break;
    }
}

static void slide_step(unsigned sf)
{
    switch (cur) {
    case 0:  step_keep(sf);     break;
    case 1:  step_kernel(sf);   break;
    case 2:  step_profiles(sf); break;
    case 3:  step_tools(sf);    break;
    case 4:  step_games(sf);    break;
    case 5:  step_metal(sf);    break;
    default: step_gate(sf);     break;
    }
}

static void go_to(int n)
{
    if (n < 0)        n = NSLIDES - 1;
    if (n >= NSLIDES) n = 0;
    cur = n;
    slide_f0 = frame;
    auto_f0  = frame;
    slide_enter();
}

/* Returns 1 when the tour should end. */
static int handle_key(int k)
{
    switch (k) {
    case KEY_ESC:
        return 1;
    case KEY_RIGHT: case KEY_SPACE: case KEY_ENTER:
    case KEY_DOWN:  case KEY_PGDN:
        go_to(cur + 1); break;
    case KEY_LEFT:  case KEY_UP:    case KEY_PGUP:
        go_to(cur - 1); break;
    case KEY_HOME:
        go_to(0); break;
    case KEY_END:
        go_to(NSLIDES - 1); break;
    case 'a': case 'A':
        autorun = !autorun;
        auto_f0 = frame;
        step_auto_meter();
        break;
    default:
        break;
    }
    return 0;
}

int main(void)
{
    unsigned long next, now;
    unsigned sf;
    int quit = 0;

    srand((unsigned)ui_ticks());
    ui_init();
    build_keep();
    build_mark();

    cur = 0; autorun = 0; frame = 0; slide_f0 = 0; auto_f0 = 0;
    slide_enter();
    next = ui_ticks() + FRAME_TICKS;

    /* Poll the BIOS buffer every turn so keys land at once, and yield
     * with ui_idle() so a real 386 or an emulator can throttle. */
    for (;;) {
        ui_idle();
        while (ui_keywaiting())
            if (handle_key(ui_getkey())) { quit = 1; break; }
        if (quit)
            break;
        now = ui_ticks();
        if (now + 20UL < next)                  /* midnight-wrap guard   */
            next = now;
        if (now < next)
            continue;
        next += FRAME_TICKS;
        frame++;
        sf = (unsigned)(frame - slide_f0);
        slide_step(sf);
        step_progress(sf);
        if ((sf % 4U) == 0U)
            step_auto_meter();
        if (autorun && (unsigned)(frame - auto_f0) >= AUTO_FRAMES)
            go_to(cur + 1);
    }

    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
