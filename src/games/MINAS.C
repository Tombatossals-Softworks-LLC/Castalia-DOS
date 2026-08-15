/* ===================================================================
 * MINAS.C  -  CASTALIA MINAS: minesweeper  (MINAS.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Minesweeper, Castalia style: the castle moat is mined and the sapper
 * (you) must chart it.  Keyboard-first cursor play, three difficulties,
 * classic colour-coded numbers, a live timer, and the traditional
 * first-reveal-is-always-safe rule (mines are placed after your first
 * move, never under it).
 *
 * 386SX friendly: cells repaint individually, the flood reveal uses a
 * fixed iterative stack (no recursion), timing is BIOS ticks.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os minas.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/UI.H"
#include "../common/SPK.H"

#define MAXW 30
#define MAXH 16
#define OY   4                  /* grid top row on screen */

static unsigned char mine[MAXH][MAXW];    /* 1 = mine                 */
static unsigned char st[MAXH][MAXW];      /* 0 hidden 1 open 2 flag   */

typedef struct { int w, h, mines; const char *label; } DIFF;
static const DIFF diffs[3] = {
    {  9,  9, 10, "Novice     9 x 9,   10 mines" },
    { 16, 16, 40, "Standard  16 x 16,  40 mines" },
    { 30, 16, 99, "Veteran   30 x 16,  99 mines" }
};

static int gw, gh, nmines, ox;
static int cx, cy, flags, opened, placed, dead;
static unsigned long t_start;

/* Classic number colours, adapted to the blue field. */
static const unsigned char numcol[9] = {
    C_LGRAY, C_LCYAN, C_LGREEN, C_LRED, C_LMAGENTA,
    C_YELLOW, C_CYAN, C_WHITE, C_DGRAY
};

static int adj(int x, int y)
{
    int dx, dy, n = 0;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx, ny = y + dy;
            if ((dx || dy) && nx >= 0 && nx < gw && ny >= 0 && ny < gh)
                n += mine[ny][nx];
        }
    return n;
}

static void draw_cell(int x, int y)
{
    int sx = ox + x * 2;
    int sy = OY + y;
    int cur = (x == cx && y == cy);
    char ch;
    unsigned char a;

    if (st[y][x] == 2) {                        /* flagged */
        ch = (char)0x10;
        a = cur ? A_ITEMSEL : UI_ATTR(C_YELLOW, C_BLUE);
    } else if (st[y][x] == 0) {                 /* hidden  */
        ch = (char)0xB1;
        a = cur ? A_ITEMSEL : UI_ATTR(C_LGRAY, C_BLUE);
    } else if (mine[y][x]) {                    /* opened mine (loss) */
        ch = (char)0x0F;
        a = cur ? A_ITEMSEL : UI_ATTR(C_WHITE, C_RED);
    } else {
        int n = adj(x, y);
        ch = n ? (char)('0' + n) : ' ';
        a = cur ? A_ITEMSEL : UI_ATTR(numcol[n], C_BLUE);
    }
    ui_putc(sx, sy, ch, a);
    ui_putc(sx + 1, sy, ' ', cur ? A_ITEMSEL : A_DESKTOP);
}

/* Wait for a key while the speaker keeps its own time: a pending note
 * only expires inside spk_poll(), so a bare ui_getkey() would hold the
 * last one until the sapper pressed something. */
static int wait_key(void)
{
    while (!ui_keywaiting()) {
        spk_poll();
        ui_idle();
    }
    spk_poll();
    return ui_getkey();
}

static void draw_hud(void)
{
    char buf[40];
    long secs = dead ? -1L : (long)((ui_ticks() - t_start) / 18UL);
    sprintf(buf, "Mines left: %-3d", nmines - flags);
    ui_puts(6, 2, buf, UI_ATTR(C_YELLOW, C_BLUE));
    sprintf(buf, "Sound: %-3s", spk_muted() ? "off" : "on");
    ui_puts(32, 2, buf, A_ITEM);
    if (secs >= 0) {
        sprintf(buf, "Time: %4ld s", secs);
        ui_puts(58, 2, buf, A_ITEM);
    }
}

static void draw_all(void)
{
    int x, y;
    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA MINAS", A_TITLE);
    ui_puts(18, 0, "Chart the moat", UI_ATTR(C_WHITE, C_BLUE));

    ui_box(ox - 2, OY - 1, gw * 2 + 3, gh + 2, A_FRAME);
    for (y = 0; y < gh; y++)
        for (x = 0; x < gw; x++)
            draw_cell(x, y);
    draw_hud();

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(1, SCR_H - 1,
        " Arrows  Enter/Space reveal  F flag  R restart  "
        "D difficulty  S sound  Esc quit", A_STATUS);
}

static void new_game(int d)
{
    gw = diffs[d].w;
    gh = diffs[d].h;
    nmines = diffs[d].mines;
    ox = (SCR_W - gw * 2) / 2 + 1;
    memset(mine, 0, sizeof(mine));
    memset(st, 0, sizeof(st));
    cx = gw / 2;
    cy = gh / 2;
    flags = 0;
    opened = 0;
    placed = 0;
    dead = 0;
    t_start = ui_ticks();
    draw_all();
}

/* Place mines AFTER the first reveal so (ax,ay) is always safe. */
static void place_mines(int ax, int ay)
{
    int n = 0, x, y;
    while (n < nmines) {
        x = rand() % gw;
        y = rand() % gh;
        if (mine[y][x] || (x == ax && y == ay))
            continue;
        mine[y][x] = 1;
        n++;
    }
    placed = 1;
    t_start = ui_ticks();
}

/* Iterative flood reveal (no recursion; fixed stack). */
static void reveal(int x, int y)
{
    static int qx[MAXW * MAXH], qy[MAXW * MAXH];
    int sp = 0;
    int dx, dy;

    qx[sp] = x; qy[sp] = y; sp++;
    while (sp > 0) {
        sp--;
        x = qx[sp]; y = qy[sp];
        if (st[y][x] != 0)
            continue;
        st[y][x] = 1;
        opened++;
        draw_cell(x, y);
        if (adj(x, y) == 0) {
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if ((dx || dy) && nx >= 0 && nx < gw &&
                        ny >= 0 && ny < gh && st[ny][nx] == 0 &&
                        !mine[ny][nx] && sp < MAXW * MAXH) {
                        qx[sp] = nx; qy[sp] = ny; sp++;
                    }
                }
        }
    }
}

static void show_mines(void)
{
    int x, y;
    for (y = 0; y < gh; y++)
        for (x = 0; x < gw; x++)
            if (mine[y][x] && st[y][x] != 2) {
                st[y][x] = 1;
                draw_cell(x, y);
            }
}

/* Returns: 1 restart, 2 change difficulty, 0 leave. */
static int end_panel(int won, long secs)
{
    int w = 44, h = 7, x = (SCR_W - w) / 2, y = 9, k;
    char buf[44];
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, won ? " The moat is charted! " : " Boom. ", A_PANELHDR);
    if (won)
        sprintf(buf, "Cleared in %ld seconds.", secs);
    else
        strcpy(buf, "A mine got the sapper.");
    ui_puts(x + 3, y + 2, buf, A_PANEL);
    ui_puts(x + 3, y + 4, "R again    D difficulty    Esc leave", A_PANEL);
    /* The field is settled and a panel is up, so blocking is fair here. */
    if (won) {
        spk_fanfare();
    } else {
        spk_tone(147, 3);
        spk_tone(98, 8);
    }
    for (;;) {
        k = wait_key();
        if (k == 'r' || k == 'R') return 1;
        if (k == 'd' || k == 'D') return 2;
        if (k == KEY_ESC) return 0;
    }
}

/* Difficulty chooser.  Returns 0..2, or -1 to quit. */
static int choose_diff(void)
{
    int sel = 1, i, k;
    for (;;) {
        ui_cls(A_DESKTOP);
        ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
        ui_puts(2, 0, "CASTALIA MINAS", A_TITLE);
        ui_center(5, "How treacherous is the moat?", A_TITLE);
        ui_box(22, 8, 36, 5, A_FRAME);
        for (i = 0; i < 3; i++) {
            unsigned char a = (i == sel) ? A_ITEMSEL : A_ITEM;
            char buf[36];
            sprintf(buf, " %-32.32s", diffs[i].label);
            ui_fill(23, 9 + i, 34, 1, ' ', a);
            ui_puts(23, 9 + i, buf, a);
        }
        ui_center(15, "The first reveal is always safe.", A_HINT);
        ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
        ui_puts(2, SCR_H - 1,
                " Up/Down Select   Enter Play   Esc Quit", A_STATUS);
        k = wait_key();
        if (k == KEY_ESC) return -1;
        if (k == KEY_UP)   sel = (sel > 0) ? sel - 1 : 2;
        if (k == KEY_DOWN) sel = (sel < 2) ? sel + 1 : 0;
        if (k == KEY_ENTER) return sel;
    }
}

int main(void)
{
    int d, key, oldx, oldy, rc;
    unsigned long last_hud = 0;

    srand((unsigned)ui_ticks());
    ui_init();
    spk_init();

    d = choose_diff();
    if (d < 0) {
        spk_off();
        ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
        ui_done();
        return 0;
    }
    new_game(d);

    for (;;) {
        /* Non-blocking wait so the timer keeps counting. */
        while (!ui_keywaiting()) {
            spk_poll();
            if (ui_ticks() - last_hud >= 18UL) {
                last_hud = ui_ticks();
                draw_hud();
            }
            ui_idle();      /* do not spin a real CPU while the clock runs */
        }
        spk_poll();
        key = ui_getkey();
        oldx = cx; oldy = cy;

        if (key == KEY_ESC)
            break;
        else if (key == KEY_UP)    { if (cy > 0) cy--; }
        else if (key == KEY_DOWN)  { if (cy < gh - 1) cy++; }
        else if (key == KEY_LEFT)  { if (cx > 0) cx--; }
        else if (key == KEY_RIGHT) { if (cx < gw - 1) cx++; }
        else if (key == 'r' || key == 'R') { new_game(d); continue; }
        else if (key == 'd' || key == 'D') {
            rc = choose_diff();
            if (rc < 0) break;
            d = rc;
            new_game(d);
            continue;
        }
        else if (key == 's' || key == 'S') {
            spk_mute(!spk_muted());
            draw_hud();
        }
        else if (key == 'f' || key == 'F') {
            if (st[cy][cx] == 0) {
                st[cy][cx] = 2; flags++;
                spk_note(1245, 2);      /* banner up   */
            } else if (st[cy][cx] == 2) {
                st[cy][cx] = 0; flags--;
                spk_note(831, 2);       /* banner down */
            }
            draw_cell(cx, cy);
            draw_hud();
        }
        else if (key == KEY_ENTER || key == KEY_SPACE) {
            if (st[cy][cx] == 0) {
                if (!placed)
                    place_mines(cx, cy);
                if (mine[cy][cx]) {
                    dead = 1;
                    st[cy][cx] = 1;
                    spk_boom();
                    show_mines();
                    draw_cell(cx, cy);
                    rc = end_panel(0, 0L);
                    if (rc == 0) break;
                    if (rc == 2) {
                        d = choose_diff();
                        if (d < 0) break;
                    }
                    new_game(d);
                    continue;
                }
                reveal(cx, cy);
                spk_blip();             /* one blip a reveal, not a cell */
                if (opened == gw * gh - nmines) {
                    long secs = (long)((ui_ticks() - t_start) / 18UL);
                    dead = 1;
                    rc = end_panel(1, secs);
                    if (rc == 0) break;
                    if (rc == 2) {
                        d = choose_diff();
                        if (d < 0) break;
                    }
                    new_game(d);
                    continue;
                }
            }
        }

        if (oldx != cx || oldy != cy) {
            draw_cell(oldx, oldy);
            draw_cell(cx, cy);
        }
    }

    spk_off();
    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
