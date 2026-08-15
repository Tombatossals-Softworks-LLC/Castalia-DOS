/* ===================================================================
 * PUZZLE.C  -  CASTALIA PUZZLE: the 15-puzzle  (PUZZLE.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * The classic sliding 15-puzzle in Castalia colours.  Shuffled with
 * random legal moves from the solved state, so every game is solvable.
 * An arrow key slides the neighbouring tile INTO the gap in that
 * direction (press Left: the tile right of the gap slides left).
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os puzzle.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/UI.H"
#include "../common/SPK.H"

static int board[16];       /* 0 = the gap */
static int gap;             /* index of the gap */
static long moves;

#define OX 22               /* board origin on screen */
#define OY 5
#define CW 9                /* cell width  */
#define CH 4                /* cell height */

static void draw_tile(int idx)
{
    int cx = OX + (idx % 4) * CW;
    int cy = OY + (idx / 4) * CH;
    int v = board[idx];
    char buf[4];

    if (v == 0) {
        ui_fill(cx, cy, CW - 1, CH - 1, ' ', A_DESKTOP);
        return;
    }
    ui_fill(cx, cy, CW - 1, CH - 1, ' ', A_PANEL);
    ui_box(cx, cy, CW - 1, CH - 1, A_PANEL);
    sprintf(buf, "%2d", v);
    ui_puts(cx + 3, cy + 1, buf,
            (v == idx + 1) ? UI_ATTR(C_GREEN, C_LGRAY) : A_PANELHDR);
}

/* The bottom hint bar, sound state included. */
static void put_status(void)
{
    char buf[80];
    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    sprintf(buf, " Arrows slide a tile into the gap   R reshuffle   "
            "S sound %s   Esc quit", spk_muted() ? "off" : "on");
    ui_puts(2, SCR_H - 1, buf, A_STATUS);
}

/* Wait for a key while the speaker keeps its own time.  A pending note
 * only ends inside spk_poll(), so a bare ui_getkey() would leave the
 * last blip droning until the player moved again. */
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
    int i;
    char buf[40];

    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA PUZZLE", A_TITLE);
    ui_puts(19, 0, "The 15-Puzzle", UI_ATTR(C_WHITE, C_BLUE));

    ui_box(OX - 2, OY - 1, 4 * CW + 3, 4 * CH + 1, A_FRAME);
    for (i = 0; i < 16; i++)
        draw_tile(i);

    sprintf(buf, "Moves: %ld    ", moves);
    ui_puts(OX, OY + 4 * CH + 1, buf, A_TITLE);

    put_status();
}

/* Slide the tile from board index 'from' into the gap.  Returns 1 if
 * a move happened. */
static int slide_from(int from)
{
    if (from < 0 || from > 15)
        return 0;
    board[gap] = board[from];
    board[from] = 0;
    draw_tile(gap);
    gap = from;
    draw_tile(gap);
    return 1;
}

/* Apply an arrow: the tile OPPOSITE the arrow slides into the gap. */
static int do_move(int key, int count_it)
{
    int gx = gap % 4, gy = gap / 4, ok = 0;

    if (key == KEY_LEFT  && gx < 3) ok = slide_from(gap + 1);
    if (key == KEY_RIGHT && gx > 0) ok = slide_from(gap - 1);
    if (key == KEY_UP    && gy < 3) ok = slide_from(gap + 4);
    if (key == KEY_DOWN  && gy > 0) ok = slide_from(gap - 4);
    if (ok && count_it)
        moves++;
    return ok;
}

static void shuffle(void)
{
    static const int keys[4] = { KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN };
    int i;
    for (i = 0; i < 16; i++)
        board[i] = (i + 1) % 16;    /* solved, gap at index 15 */
    gap = 15;
    for (i = 0; i < 400; i++)
        do_move(keys[rand() % 4], 0);
    moves = 0;
}

static int solved(void)
{
    int i;
    for (i = 0; i < 15; i++)
        if (board[i] != i + 1)
            return 0;
    return 1;
}

static void win_box(void)
{
    int w = 40, h = 7, x = (SCR_W - w) / 2, y = 9;
    char buf[40];
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " The keep is in order! ", A_PANELHDR);
    sprintf(buf, "Solved in %ld moves.", moves);
    ui_puts(x + 3, y + 2, buf, A_PANEL);
    ui_puts(x + 3, y + 4, "R play again      Esc leave", A_PANEL);
    spk_fanfare();                  /* blocking: the puzzle is done */
}

int main(void)
{
    int key, playing = 1;

    srand((unsigned)ui_ticks());
    ui_init();
    spk_init();
    shuffle();
    draw_all();

    for (;;) {
        key = wait_key();
        if (key == KEY_ESC)
            break;
        else if (key == 'r' || key == 'R') {
            shuffle();
            playing = 1;
            draw_all();
        } else if (key == 's' || key == 'S') {
            spk_mute(!spk_muted());
            put_status();
        } else if (playing &&
                   (key == KEY_LEFT || key == KEY_RIGHT ||
                    key == KEY_UP || key == KEY_DOWN)) {
            if (do_move(key, 1)) {
                char buf[24];
                spk_blip();             /* one tile, one click */
                sprintf(buf, "Moves: %ld    ", moves);
                ui_puts(OX, OY + 4 * CH + 1, buf, A_TITLE);
                if (solved()) {
                    playing = 0;
                    win_box();
                }
            } else {
                spk_bad();              /* the gap is against that wall */
            }
        }
    }

    spk_off();
    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
