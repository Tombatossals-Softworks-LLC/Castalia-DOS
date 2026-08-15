/* ===================================================================
 * BARRELS.C  -  CASTALIA BARRELS  (BARRELS.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * A Sokoban set in a fortress cellar: shove the barrels onto their
 * marked storage spots.  You may push one barrel at a time and never
 * into a wall or another barrel; you can never pull.  Clear every spot
 * to open the next chamber.
 *
 * Turn-based, so input blocks on the BIOS keyboard.  No dynamic memory:
 * levels are C string tables parsed into fixed grids, and a bounded
 * history stack makes every move undoable.  386SX friendly.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os barrels.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/UI.H"
#include "../common/SPK.H"

#define MAXW     24         /* widest level we can hold        */
#define MAXH     18         /* tallest level we can hold        */
#define NLEVELS  8
#define HISTMAX  512        /* bounded undo stack               */

/* Cellar palette: stone-grey walls on black, amber accents. */
#define A_WALL     UI_ATTR(C_LGRAY,  C_BLACK)
#define A_FLOOR    UI_ATTR(C_DGRAY,  C_BLACK)
#define A_TGT      UI_ATTR(C_LRED,   C_BLACK)
#define A_BARREL   UI_ATTR(C_BROWN,  C_BLACK)
#define A_BARDONE  UI_ATTR(C_YELLOW, C_BLACK)   /* barrel on its spot */
#define A_PLAYER   UI_ATTR(C_WHITE,  C_BLACK)

/* CP437 glyphs. */
#define CH_WALL    ((char)0xB2)     /* dark shade block  */
#define CH_FLOOR   ((char)0xFA)     /* centred dot       */
#define CH_TGT     ((char)0x04)     /* diamond           */
#define CH_BARREL  ((char)0x09)     /* circle            */
#define CH_PLAYER  ((char)0x02)     /* smiley            */

/* Level maps.  Standard Sokoban charset:
 *   # wall   ' ' floor   @ player   $ barrel   . target
 *   * barrel-on-target   + player-on-target
 * Every level is fully wall-bordered, has one player, and an equal
 * number of barrels and targets.  Each has been checked solvable. */
static const char *levels[NLEVELS][MAXH] = {
    {   /* 1 - a single shove */
        "#######",
        "#     #",
        "# @$. #",
        "#     #",
        "#######"
    },
    {   /* 2 - two barrels, mind the order */
        "######",
        "#    #",
        "# $. #",
        "# .$ #",
        "#  @ #",
        "######"
    },
    {   /* 3 - lift two into the rack */
        "#######",
        "#   ..#",
        "#   $$#",
        "#@    #",
        "#######"
    },
    {   /* 4 - the L-turn: left, then up */
        "#######",
        "#     #",
        "#.### #",
        "#.$   #",
        "# $ @ #",
        "#     #",
        "#######"
    },
    {   /* 5 - one way round the pillar */
        "#######",
        "#     #",
        "#.$   #",
        "###$# #",
        "#  .  #",
        "#   @ #",
        "#######"
    },
    {   /* 6 - the four-spoke room */
        "#######",
        "#  .  #",
        "# .$. #",
        "# $@$ #",
        "#     #",
        "#######"
    },
    {   /* 7 - four barrels, four corners */
        "#########",
        "#       #",
        "# .$ $. #",
        "#   @   #",
        "# .$ $. #",
        "#       #",
        "#########"
    },
    {   /* 8 - shove each to a far corner */
        "#######",
        "#. .  #",
        "#  $  #",
        "# $@$ #",
        "#  $  #",
        "#  . .#",
        "#######"
    }
};

/* Working grids for the current level. */
static unsigned char wall[MAXH][MAXW];
static unsigned char tgt[MAXH][MAXW];    /* static storage-spot layer */
static unsigned char bar[MAXH][MAXW];    /* dynamic barrel layer      */
static int px, py;                       /* player position           */
static int lw, lh, ox, oy;               /* level size and origin     */
static int level, moves, pushes;

/* Undo history: direction of the move and whether it pushed a barrel. */
static int h_dx[HISTMAX], h_dy[HISTMAX], h_push[HISTMAX];
static int histn;

static void record(int dx, int dy, int pushed)
{
    if (histn >= HISTMAX) {              /* drop the oldest entry */
        int i;
        for (i = 1; i < HISTMAX; i++) {
            h_dx[i - 1] = h_dx[i];
            h_dy[i - 1] = h_dy[i];
            h_push[i - 1] = h_push[i];
        }
        histn = HISTMAX - 1;
    }
    h_dx[histn] = dx;
    h_dy[histn] = dy;
    h_push[histn] = pushed;
    histn++;
}

static void load_level(int idx)
{
    const char **rows = levels[idx];
    int x, y, len, rlen;

    memset(wall, 0, sizeof(wall));
    memset(tgt, 0, sizeof(tgt));
    memset(bar, 0, sizeof(bar));
    px = py = 0;

    lh = 0;
    while (lh < MAXH && rows[lh])
        lh++;
    lw = 0;
    for (y = 0; y < lh; y++) {
        len = (int)strlen(rows[y]);
        if (len > lw)
            lw = len;
    }
    for (y = 0; y < lh; y++) {
        rlen = (int)strlen(rows[y]);
        for (x = 0; x < lw; x++) {
            char c = (x < rlen) ? rows[y][x] : '#';
            switch (c) {
            case '#': wall[y][x] = 1; break;
            case '.': tgt[y][x] = 1; break;
            case '$': bar[y][x] = 1; break;
            case '*': bar[y][x] = 1; tgt[y][x] = 1; break;
            case '@': px = x; py = y; break;
            case '+': px = x; py = y; tgt[y][x] = 1; break;
            default:  break;             /* space -> floor */
            }
        }
    }

    ox = (SCR_W - lw) / 2;
    oy = (SCR_H - lh) / 2;
    if (oy < 4)
        oy = 4;
    moves = 0;
    pushes = 0;
    histn = 0;
}

static void draw_cell(int x, int y)
{
    int sx = ox + x, sy = oy + y;
    char ch;
    unsigned char a;

    if (wall[y][x]) {
        ch = CH_WALL;  a = A_WALL;
    } else if (x == px && y == py) {
        ch = CH_PLAYER; a = A_PLAYER;
    } else if (bar[y][x]) {
        ch = CH_BARREL;
        a = tgt[y][x] ? A_BARDONE : A_BARREL;
    } else if (tgt[y][x]) {
        ch = CH_TGT;   a = A_TGT;
    } else {
        ch = CH_FLOOR; a = A_FLOOR;
    }
    ui_putc(sx, sy, ch, a);
}

static void draw_hud(void)
{
    char buf[40];
    ui_fill(0, 1, SCR_W, 1, ' ', A_DESKTOP);
    sprintf(buf, "Chamber %d/%d    Moves %d    Pushes %d",
            level + 1, NLEVELS, moves, pushes);
    ui_puts(2, 1, buf, A_TITLE);
}

static void draw_board(void)
{
    int x, y;
    for (y = 0; y < lh; y++)
        for (x = 0; x < lw; x++)
            draw_cell(x, y);
    draw_hud();
}

/* The bottom hint bar, sound state included. */
static void draw_status(void)
{
    char buf[80];
    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    sprintf(buf, " Arrows push   Bksp undo   R reset   N next   "
            "S sound %s   Esc quit", spk_muted() ? "off" : "on");
    ui_puts(1, SCR_H - 1, buf, A_STATUS);
}

/* Wait for a key while the speaker keeps its own time.  The cellar is
 * turn-based, so a bare ui_getkey() would hold the last note until the
 * next shove; spk_poll() retires it on schedule instead. */
static int wait_key(void)
{
    while (!ui_keywaiting()) {
        spk_poll();
        ui_idle();
    }
    spk_poll();
    return ui_getkey();
}

static void draw_all(void)
{
    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA BARRELS  the fortress cellar", A_TITLE);
    draw_status();
    draw_board();
}

/* Attempt to step/push in (dx,dy).  Returns 1 if something moved. */
static int do_move(int dx, int dy)
{
    int nx = px + dx, ny = py + dy;

    if (wall[ny][nx])
        return 0;
    if (bar[ny][nx]) {
        int bx = nx + dx, by = ny + dy;
        if (wall[by][bx] || bar[by][bx])
            return 0;                    /* can't push into wall/barrel */
        bar[ny][nx] = 0;
        bar[by][bx] = 1;
        if (tgt[by][bx])
            spk_ok();                    /* the barrel found its mark   */
        else
            spk_blip();                  /* an honest shove             */
        record(dx, dy, 1);
        px = nx; py = ny;
        pushes++;
        moves++;
        return 1;
    }
    record(dx, dy, 0);
    px = nx; py = ny;
    moves++;
    return 1;
}

/* Reverse the most recent move, restoring any barrel it pushed. */
static void undo(void)
{
    int dx, dy, pushed;
    if (histn == 0)
        return;
    histn--;
    dx = h_dx[histn];
    dy = h_dy[histn];
    pushed = h_push[histn];

    if (pushed) {                        /* barrel sat ahead of player */
        bar[py + dy][px + dx] = 0;
        bar[py][px] = 1;
        pushes--;
    }
    px -= dx;
    py -= dy;
    moves--;
}

static int is_won(void)
{
    int x, y;
    for (y = 0; y < lh; y++)
        for (x = 0; x < lw; x++)
            if (tgt[y][x] && !bar[y][x])
                return 0;
    return 1;
}

static void win_panel(void)
{
    int w = 42, h = 6, x = (SCR_W - w) / 2, y = 9;
    char buf[42];
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " Chamber cleared! ", A_PANELHDR);
    sprintf(buf, "Barrels stowed in %d pushes.", pushes);
    ui_puts(x + 3, y + 2, buf, A_PANEL);
    ui_puts(x + 3, y + 4, "Press any key for the next chamber.", A_PANEL);
    spk_fanfare();                       /* blocking: the chamber is done */
    wait_key();
}

/* The final panel.  Returns 1 to replay from the first chamber, 0 to leave. */
static int final_panel(void)
{
    int w = 44, h = 7, x = (SCR_W - w) / 2, y = 9, k;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " You cleared the cellar! ", A_PANELHDR);
    ui_puts(x + 3, y + 2, "Every barrel is on its mark.", A_PANEL);
    ui_puts(x + 3, y + 4, "R replay from the first chamber", A_PANEL);
    ui_puts(x + 3, y + 5, "Esc leave the cellar", A_PANEL);
    spk_fanfare();                       /* blocking: the cellar is done */
    for (;;) {
        k = wait_key();
        if (k == 'r' || k == 'R')
            return 1;
        if (k == KEY_ESC)
            return 0;
    }
}

int main(void)
{
    int key;

    ui_init();
    spk_init();
    level = 0;
    load_level(level);
    draw_all();

    for (;;) {
        key = wait_key();

        if (key == KEY_ESC) {
            break;
        } else if (key == 's' || key == 'S') {
            spk_mute(!spk_muted());
            draw_status();
        } else if (key == 'r' || key == 'R') {
            load_level(level);
            draw_all();
        } else if (key == 'n' || key == 'N') {
            level = (level + 1) % NLEVELS;
            load_level(level);
            draw_all();
        } else if (key == KEY_BKSP) {
            undo();
            draw_board();
        } else if (key == KEY_UP || key == KEY_DOWN ||
                   key == KEY_LEFT || key == KEY_RIGHT) {
            int dx = 0, dy = 0;
            if (key == KEY_UP)         dy = -1;
            else if (key == KEY_DOWN)  dy = 1;
            else if (key == KEY_LEFT)  dx = -1;
            else                       dx = 1;

            if (do_move(dx, dy)) {
                draw_board();
                if (is_won()) {
                    if (level == NLEVELS - 1) {
                        if (final_panel()) {
                            level = 0;
                            load_level(level);
                            draw_all();
                        } else {
                            break;
                        }
                    } else {
                        win_panel();
                        level++;
                        load_level(level);
                        draw_all();
                    }
                }
            } else {
                spk_bad();               /* nothing gives in that direction */
            }
        }
    }

    spk_off();
    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
