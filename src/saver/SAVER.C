/* ===================================================================
 * SAVER.C  -  CASTALIA NIGHT  (SAVER.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * A calm, looping text-mode screensaver: the Castalia fortress as a
 * dark silhouette under a starfield.  Stars twinkle, a crescent moon
 * drifts overhead, the keep's windows flicker like candlelight, a
 * pennant flutters amber/red, and now and then a shooting star streaks
 * across the sky.  The "CASTALIA DOS" wordmark drifts gently so nothing
 * burns into a phosphor.
 *
 * Built for a 386SX: everything is drawn incrementally - a full repaint
 * only happens once, at start - the loop is paced off the BIOS tick and
 * yields with ui_idle() so it never spins a real CPU or an emulator.
 * Any key wakes the keep and exits clean.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os saver.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/UI.H"

/* --- layout ---------------------------------------------------------- */
#define NSTARS      64          /* twinkling stars in the sky           */
#define NWIN        24          /* candle-lit keep windows              */
#define STAR_YMAX   13          /* stars/shooting stars stay above wall */
#define MOON_Y      3           /* crescent sits high in the sky        */
#define PENNANT_X   40          /* flag over the central keep           */
#define PENNANT_Y   14
#define TR          4           /* shooting-star trail length           */
#define FRAME_TICKS 2UL         /* one animation frame per ~2 BIOS ticks */

/* Five towers across the skyline: left edge, right edge, top row.
 * Index 2 is the tall central keep. */
static const int tw_x0[5]  = {  6, 22, 36, 52, 66 };
static const int tw_x1[5]  = { 14, 30, 46, 60, 74 };
static const int tw_top[5] = { 18, 17, 15, 17, 18 };

/* --- fortress silhouette --------------------------------------------- */
static int          fort_top[SCR_W];    /* topmost solid row per column  */
static int          win_x[NWIN], win_y[NWIN];
static unsigned char win_on[NWIN];      /* 2 bright, 1 dim, 0 out        */
static int          nwin;

/* --- starfield ------------------------------------------------------- */
static int          star_x[NSTARS], star_y[NSTARS];
static char         star_ch[NSTARS];
static unsigned char star_at[NSTARS];

/* --- moving decorations ---------------------------------------------- */
static int          moon_x, moon_dir;
static const char  *MARK = "CASTALIA DOS";
static int          mark_x, mark_y, mark_dir, marklen;
static int          pen_alt;
static const char  *HINT = "press any key";
static int          hint_x, hint_on, hintlen;

/* --- shooting star --------------------------------------------------- */
static int          sh_active, sh_dx;
static int          sh_tx[TR], sh_ty[TR];

/* Is (x,y) currently hidden beneath a bright moving element?  Used so a
 * twinkling star never poke a hole through the moon, wordmark or hint. */
static int covered(int x, int y)
{
    if (x == moon_x && (y == MOON_Y - 1 || y == MOON_Y || y == MOON_Y + 1))
        return 1;
    if (y == mark_y && x >= mark_x && x < mark_x + marklen)
        return 1;
    if (hint_on && y == 0 && x >= hint_x && x < hint_x + hintlen)
        return 1;
    return 0;
}

/* Roll a fresh brightness/colour for star i (does not draw it). */
static void set_star(int i)
{
    int r = rand() % 100;
    if (r < 45)      { star_ch[i] = (char)0xFA; star_at[i] = UI_ATTR(C_DGRAY,  C_BLACK); }
    else if (r < 75) { star_ch[i] = (char)0xF9; star_at[i] = UI_ATTR(C_LGRAY,  C_BLACK); }
    else if (r < 90) { star_ch[i] = (char)'*';  star_at[i] = UI_ATTR(C_WHITE,  C_BLACK); }
    else if (r < 96) { star_ch[i] = (char)0x0F; star_at[i] = UI_ATTR(C_YELLOW, C_BLACK); }
    else             { star_ch[i] = (char)0x0F; star_at[i] = UI_ATTR(C_LBLUE,  C_BLACK); }
}

static void draw_star(int i)
{
    if (!covered(star_x[i], star_y[i]))
        ui_putc(star_x[i], star_y[i], star_ch[i], star_at[i]);
}

static void draw_window(int i)
{
    unsigned char a;
    if (win_on[i] == 2)      a = UI_ATTR(C_YELLOW, C_BLACK);
    else if (win_on[i] == 1) a = UI_ATTR(C_BROWN,  C_BLACK);
    else                     a = UI_ATTR(C_DGRAY,  C_BLACK);
    ui_putc(win_x[i], win_y[i], (char)0xDB, a);
}

static void draw_pennant(void)
{
    ui_putc(PENNANT_X, PENNANT_Y, (char)0x1E,
            pen_alt ? UI_ATTR(C_YELLOW, C_BLACK)
                    : UI_ATTR(C_LRED,   C_BLACK));
}

static void draw_moon(void)
{
    ui_putc(moon_x, MOON_Y - 1, (char)')', UI_ATTR(C_WHITE, C_BLACK));
    ui_putc(moon_x, MOON_Y,     (char)')', UI_ATTR(C_WHITE, C_BLACK));
    ui_putc(moon_x, MOON_Y + 1, (char)')', UI_ATTR(C_WHITE, C_BLACK));
}

static void draw_wordmark(void)
{
    ui_puts(mark_x, mark_y, MARK, UI_ATTR(C_YELLOW, C_BLACK));
}

static void draw_hint(void)
{
    if (hint_on)
        ui_puts(hint_x, 0, HINT, UI_ATTR(C_DGRAY, C_BLACK));
}

/* Reconstruct whatever *belongs* at (x,y) after a moving element leaves
 * it.  Priority runs front-to-back: fortress, pennant, wordmark, moon,
 * hint, star, then empty sky. */
static void restore_cell(int x, int y)
{
    int i;
    if (x < 0 || x >= SCR_W || y < 0 || y >= SCR_H)
        return;
    if (y >= fort_top[x]) {
        for (i = 0; i < nwin; i++)
            if (win_x[i] == x && win_y[i] == y) { draw_window(i); return; }
        ui_putc(x, y, (char)0xDB, UI_ATTR(C_DGRAY, C_BLACK));
        return;
    }
    if (x == PENNANT_X && y == PENNANT_Y) { draw_pennant(); return; }
    if (y == mark_y && x >= mark_x && x < mark_x + marklen) {
        ui_putc(x, y, MARK[x - mark_x], UI_ATTR(C_YELLOW, C_BLACK));
        return;
    }
    if (x == moon_x && (y == MOON_Y - 1 || y == MOON_Y || y == MOON_Y + 1)) {
        ui_putc(x, y, (char)')', UI_ATTR(C_WHITE, C_BLACK));
        return;
    }
    if (hint_on && y == 0 && x >= hint_x && x < hint_x + hintlen) {
        ui_putc(x, 0, HINT[x - hint_x], UI_ATTR(C_DGRAY, C_BLACK));
        return;
    }
    for (i = 0; i < NSTARS; i++)
        if (star_x[i] == x && star_y[i] == y) {
            ui_putc(x, y, star_ch[i], star_at[i]);
            return;
        }
    ui_putc(x, y, ' ', UI_ATTR(C_LGRAY, C_BLACK));
}

/* Compute the crenellated skyline and pick the lit window cells. */
static void build_fortress(void)
{
    int c, t;
    for (c = 0; c < SCR_W; c++)
        fort_top[c] = 21;                       /* curtain wall          */
    for (t = 0; t < 5; t++)
        for (c = tw_x0[t]; c <= tw_x1[t]; c++)
            if (tw_top[t] < fort_top[c])
                fort_top[c] = tw_top[t];         /* towers rise above it  */
    for (c = 0; c < SCR_W; c++)
        if (c & 1)
            fort_top[c] += 1;                    /* merlons / crenels     */

    nwin = 0;
    for (t = 0; t < 5; t++) {
        int rows[2], cols[3], nc, ri, ci;
        rows[0] = tw_top[t] + 2;
        rows[1] = tw_top[t] + 4;
        cols[0] = tw_x0[t] + 2;
        cols[1] = tw_x1[t] - 2;
        nc = 2;
        if (t == 2) { cols[2] = (tw_x0[t] + tw_x1[t]) / 2; nc = 3; }
        for (ri = 0; ri < 2; ri++)
            for (ci = 0; ci < nc; ci++)
                if (rows[ri] <= SCR_H - 2 && nwin < NWIN) {
                    win_x[nwin] = cols[ci];
                    win_y[nwin] = rows[ri];
                    win_on[nwin] = 2;
                    nwin++;
                }
    }
}

static void draw_fortress(void)
{
    int c, r, i;
    for (c = 0; c < SCR_W; c++)
        for (r = fort_top[c]; r < SCR_H; r++)
            ui_putc(c, r, (char)0xDB, UI_ATTR(C_DGRAY, C_BLACK));
    for (i = 0; i < nwin; i++)
        draw_window(i);
}

static void init_stars(void)
{
    int i;
    for (i = 0; i < NSTARS; i++) {
        star_x[i] = rand() % SCR_W;
        star_y[i] = rand() % (STAR_YMAX + 1);
        set_star(i);
    }
}

/* The moon glides slowly and bounces off the sky edges. */
static void drift_moon(void)
{
    int old = moon_x, y;
    moon_x += moon_dir;
    if (moon_x <= 4)          { moon_x = 4;          moon_dir = 1;  }
    if (moon_x >= SCR_W - 6)  { moon_x = SCR_W - 6;  moon_dir = -1; }
    for (y = MOON_Y - 1; y <= MOON_Y + 1; y++)
        restore_cell(old, y);
    draw_moon();
}

/* The wordmark drifts left-right in its band so it never burns in. */
static void drift_wordmark(void)
{
    int old = mark_x, x;
    mark_x += mark_dir;
    if (mark_x <= 1)                      { mark_x = 1;                    mark_dir = 1;  }
    if (mark_x >= SCR_W - marklen - 1)    { mark_x = SCR_W - marklen - 1;  mark_dir = -1; }
    for (x = old; x < old + marklen; x++)
        restore_cell(x, mark_y);
    draw_wordmark();
}

static void shoot_spawn(void)
{
    int i;
    sh_active = 1;
    sh_tx[0] = 8 + rand() % 60;
    sh_ty[0] = rand() % 4;
    sh_dx = (rand() & 1) ? 1 : -1;
    for (i = 1; i < TR; i++) { sh_tx[i] = sh_tx[0]; sh_ty[i] = sh_ty[0]; }
}

/* Advance the streak one cell, dragging a short fading trail behind it. */
static void shoot_step(void)
{
    int nx = sh_tx[0] + sh_dx;
    int ny = sh_ty[0] + 1;
    int i;
    if (nx < 0 || nx >= SCR_W || ny > STAR_YMAX) {
        for (i = 0; i < TR; i++)
            restore_cell(sh_tx[i], sh_ty[i]);
        sh_active = 0;
        return;
    }
    restore_cell(sh_tx[TR - 1], sh_ty[TR - 1]);
    for (i = TR - 1; i > 0; i--) { sh_tx[i] = sh_tx[i - 1]; sh_ty[i] = sh_ty[i - 1]; }
    sh_tx[0] = nx; sh_ty[0] = ny;
    ui_putc(sh_tx[0], sh_ty[0], (char)'*',  UI_ATTR(C_WHITE, C_BLACK));
    ui_putc(sh_tx[1], sh_ty[1], (char)0x0F, UI_ATTR(C_LGRAY, C_BLACK));
    ui_putc(sh_tx[2], sh_ty[2], (char)0xFA, UI_ATTR(C_LGRAY, C_BLACK));
    ui_putc(sh_tx[3], sh_ty[3], (char)0xFA, UI_ATTR(C_DGRAY, C_BLACK));
}

static void hide_hint(void)
{
    int x;
    for (x = hint_x; x < hint_x + hintlen; x++)
        restore_cell(x, 0);
}

/* One tick of the world: twinkle, candle-flicker, flutter, drift, streak. */
static void frame_step(unsigned f)
{
    int k, i, lvl, r;

    for (k = 0; k < 5; k++) {               /* stars twinkle             */
        i = rand() % NSTARS;
        set_star(i);
        draw_star(i);
    }

    if ((f % 2U) == 0U) {                    /* candlelight in the keep   */
        for (k = 0; k < 3; k++) {
            i = rand() % nwin;
            r = rand() % 100;
            lvl = (r < 70) ? 2 : (r < 94) ? 1 : 0;
            win_on[i] = (unsigned char)lvl;
            draw_window(i);
        }
    }

    if ((f % 9U) == 0U) {                    /* pennant flutters          */
        pen_alt = !pen_alt;
        draw_pennant();
    }

    if ((f % 6U) == 0U)  drift_wordmark();   /* gentle wordmark drift     */
    if ((f % 55U) == 0U) drift_moon();       /* slow moonrise             */

    if (sh_active)                           /* the occasional streak     */
        shoot_step();
    else if (rand() % 45 == 0)
        shoot_spawn();

    if ((f % 40U) == 0U) {                   /* the hint fades and hops   */
        if (hint_on) {
            hide_hint();
            hint_on = 0;
        } else {
            hint_x = (SCR_W - hintlen) / 2 + (rand() % 7 - 3);
            hint_on = 1;
            draw_hint();
        }
    }
}

/* The one and only full repaint - everything after this is incremental. */
static void paint_all(void)
{
    int i;
    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    draw_fortress();
    for (i = 0; i < NSTARS; i++)
        draw_star(i);
    draw_moon();
    draw_wordmark();
    draw_pennant();
    draw_hint();
}

int main(void)
{
    unsigned long next, now;
    unsigned int frame;

    marklen = (int)strlen(MARK);
    hintlen = (int)strlen(HINT);

    moon_x = 60; moon_dir = -1;
    mark_x = (SCR_W - marklen) / 2; mark_y = 8; mark_dir = 1;
    hint_x = (SCR_W - hintlen) / 2; hint_on = 1;
    pen_alt = 0; sh_active = 0; nwin = 0;

    srand((unsigned)ui_ticks());
    ui_init();

    build_fortress();
    init_stars();
    paint_all();

    frame = 0;
    next = ui_ticks() + FRAME_TICKS;

    /* Yield every turn (ui_idle) so a real 386 or an emulator can idle;
     * poll the BIOS buffer every turn so any key exits instantly. */
    for (;;) {
        ui_idle();
        if (ui_keywaiting()) { (void)ui_getkey(); break; }
        now = ui_ticks();
        if (now + 20UL < next)              /* midnight-wrap guard       */
            next = now;
        if (now >= next) {
            next += FRAME_TICKS;
            frame++;
            frame_step(frame);
        }
    }

    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
