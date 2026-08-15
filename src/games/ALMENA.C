/* ===================================================================
 * ALMENA.C  -  CASTALIA ALMENA: falling blocks  (ALMENA.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * "Almena" is Spanish for a battlement merlon: stack the falling
 * stones into solid rows to keep the castle wall whole.  A classic
 * falling-blocks game in Castalia colours: seven pieces, rotation with
 * a simple wall-kick, soft and hard drops, line flash, levels that
 * speed up every ten lines.
 *
 * 386SX friendly: incremental drawing (only the moving piece repaints
 * per step; the well repaints in full only after a line clear), BIOS
 * tick pacing, keyboard polling through the BIOS buffer, no TSRs.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os almena.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/UI.H"
#include "../common/SPK.H"

/* Well geometry: 10 x 18 cells, each cell two characters wide. */
#define WW 10
#define WH 18
#define WX 30               /* screen column of the well interior */
#define WY 3                /* screen row of the well interior    */

static unsigned char well[WH][WW];   /* 0 empty, else colour idx 1..7 */

/* --- Pieces -----------------------------------------------------------
 * Base cell lists per piece; the other rotations are generated at
 * startup with (x,y) -> (size-1-y, x) inside the piece's own box. */
typedef struct { int x[4], y[4]; } ROT;
static ROT shape[7][4];

static const int base_size[7] = { 4, 2, 3, 3, 3, 3, 3 };
static const int base_x[7][4] = {
    { 0, 1, 2, 3 },     /* I */
    { 0, 1, 0, 1 },     /* O */
    { 0, 1, 2, 1 },     /* T */
    { 1, 2, 0, 1 },     /* S */
    { 0, 1, 1, 2 },     /* Z */
    { 0, 0, 1, 2 },     /* J */
    { 2, 0, 1, 2 }      /* L */
};
static const int base_y[7][4] = {
    { 1, 1, 1, 1 },
    { 0, 0, 1, 1 },
    { 1, 1, 1, 2 },
    { 0, 0, 1, 1 },
    { 0, 0, 1, 1 },
    { 0, 1, 1, 1 },
    { 0, 1, 1, 1 }
};
static const unsigned char piece_fg[7] = {
    C_LCYAN, C_YELLOW, C_LMAGENTA, C_LGREEN, C_LRED, C_LBLUE, C_BROWN
};

static void init_shapes(void)
{
    int p, r, i;
    for (p = 0; p < 7; p++) {
        for (i = 0; i < 4; i++) {
            shape[p][0].x[i] = base_x[p][i];
            shape[p][0].y[i] = base_y[p][i];
        }
        for (r = 1; r < 4; r++) {
            for (i = 0; i < 4; i++) {
                shape[p][r].x[i] =
                    base_size[p] - 1 - shape[p][r - 1].y[i];
                shape[p][r].y[i] = shape[p][r - 1].x[i];
            }
        }
    }
}

/* --- Game state -------------------------------------------------------- */

static int cur_p, cur_r, cur_x, cur_y;
static int next_p;
static long score;
static int lines, level;

static int fall_ticks(void)
{
    int t = 9 - level;
    return (t < 1) ? 1 : t;
}

/* --- Drawing ------------------------------------------------------------ */

static void draw_cell(int wx, int wy, int colidx)
{
    int sx = WX + wx * 2;
    int sy = WY + wy;
    if (wy < 0)
        return;
    if (colidx == 0) {
        ui_putc(sx, sy, ' ', A_DESKTOP);
        ui_putc(sx + 1, sy, ' ', A_DESKTOP);
    } else {
        unsigned char a = UI_ATTR(piece_fg[colidx - 1], C_BLUE);
        ui_putc(sx, sy, (char)0xDB, a);
        ui_putc(sx + 1, sy, (char)0xDB, a);
    }
}

static void draw_piece(int erase)
{
    int i;
    for (i = 0; i < 4; i++)
        draw_cell(cur_x + shape[cur_p][cur_r].x[i],
                  cur_y + shape[cur_p][cur_r].y[i],
                  erase ? 0 : cur_p + 1);
}

static void draw_well(void)
{
    int x, y;
    for (y = 0; y < WH; y++)
        for (x = 0; x < WW; x++)
            draw_cell(x, y, well[y][x]);
}

static void draw_hud(void)
{
    char buf[24];
    sprintf(buf, "Score  %-8ld", score);
    ui_puts(9, 6, buf, UI_ATTR(C_YELLOW, C_BLUE));
    sprintf(buf, "Lines  %-6d", lines);
    ui_puts(9, 8, buf, A_ITEM);
    sprintf(buf, "Level  %-4d", level);
    ui_puts(9, 10, buf, A_ITEM);
}

static void draw_next(void)
{
    int i;
    ui_fill(58, 5, 8, 4, ' ', A_DESKTOP);
    for (i = 0; i < 4; i++) {
        int sx = 58 + shape[next_p][0].x[i] * 2;
        int sy = 5 + shape[next_p][0].y[i];
        unsigned char a = UI_ATTR(piece_fg[next_p], C_BLUE);
        ui_putc(sx, sy, (char)0xDB, a);
        ui_putc(sx + 1, sy, (char)0xDB, a);
    }
}

/* The sound line of the key legend, redrawn whenever S is pressed. */
static void draw_sound_hint(void)
{
    char buf[24];
    sprintf(buf, "S         sound %-3s", spk_muted() ? "off" : "on");
    ui_puts(54, 17, buf, A_HINT);
}

/* Wait for a key while the speaker keeps its own time: a pending note
 * only expires inside spk_poll(), so a bare ui_getkey() would hold the
 * last one until the player pressed something. */
static int wait_key(void)
{
    while (!ui_keywaiting()) {
        spk_poll();
        ui_idle();
    }
    spk_poll();
    return ui_getkey();
}

static void draw_screen(void)
{
    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA ALMENA", A_TITLE);
    ui_puts(19, 0, "Keep the wall whole", UI_ATTR(C_WHITE, C_BLUE));

    ui_box(7, 4, 18, 9, A_FRAME);
    ui_puts(9, 4, " Ledger ", A_TITLE);

    ui_box(WX - 1, WY - 1, WW * 2 + 2, WH + 2, A_FRAME);
    ui_puts(WX + 5, WY - 1, " Almena ", A_TITLE);

    ui_box(56, 4, 12, 6, A_FRAME);
    ui_puts(58, 4, " Next ", A_TITLE);

    ui_puts(54, 12, "Left/Right move", A_HINT);
    ui_puts(54, 13, "Up        rotate", A_HINT);
    ui_puts(54, 14, "Down      soft drop", A_HINT);
    ui_puts(54, 15, "Space     hard drop", A_HINT);
    ui_puts(54, 16, "P         pause", A_HINT);
    draw_sound_hint();                              /* row 17 */
    ui_puts(54, 18, "Esc       leave", A_HINT);

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(2, SCR_H - 1,
            " Stack the stones into solid rows to mend the battlements.",
            A_STATUS);
    draw_well();
    draw_hud();
    draw_next();
}

/* --- Mechanics ------------------------------------------------------------ */

static int collide(int p, int r, int px, int py)
{
    int i, wx, wy;
    for (i = 0; i < 4; i++) {
        wx = px + shape[p][r].x[i];
        wy = py + shape[p][r].y[i];
        if (wx < 0 || wx >= WW || wy >= WH)
            return 1;
        if (wy >= 0 && well[wy][wx])
            return 1;
    }
    return 0;
}

static void lock_piece(void)
{
    int i, wx, wy;
    for (i = 0; i < 4; i++) {
        wx = cur_x + shape[cur_p][cur_r].x[i];
        wy = cur_y + shape[cur_p][cur_r].y[i];
        if (wy >= 0 && wy < WH && wx >= 0 && wx < WW)
            well[wy][wx] = (unsigned char)(cur_p + 1);
    }
    spk_note(392, 1);                   /* the stone settles: one tick */
}

static void wait_ticks(int n)
{
    unsigned long until = ui_ticks() + (unsigned long)n;
    while (ui_ticks() < until) {
        spk_poll();
        if (ui_ticks() + 100UL < until)
            break;                      /* midnight wrap guard */
    }
    spk_poll();
}

static void clear_lines(void)
{
    int full[4], nfull = 0;
    int x, y, f, k;
    static const long pts[5] = { 0L, 100L, 300L, 500L, 800L };
    /* One note per clear, higher and longer the more rows fall. */
    static const unsigned tone[5] = { 0U, 880U, 1047U, 1245U, 1568U };

    for (y = 0; y < WH; y++) {
        int all = 1;
        for (x = 0; x < WW; x++)
            if (!well[y][x]) { all = 0; break; }
        if (all && nfull < 4)
            full[nfull++] = y;
    }
    if (nfull == 0)
        return;
    spk_note(tone[nfull], nfull + 1);

    /* Flash the completed rows. */
    for (f = 0; f < 4; f++) {
        for (k = 0; k < nfull; k++) {
            for (x = 0; x < WW; x++) {
                int sx = WX + x * 2;
                unsigned char a = (f & 1)
                    ? UI_ATTR(piece_fg[well[full[k]][x] - 1], C_BLUE)
                    : UI_ATTR(C_WHITE, C_BLUE);
                ui_putc(sx, WY + full[k], (char)0xDB, a);
                ui_putc(sx + 1, WY + full[k], (char)0xDB, a);
            }
        }
        wait_ticks(2);
    }

    /* Collapse. */
    for (k = 0; k < nfull; k++) {
        for (y = full[k]; y > 0; y--)
            for (x = 0; x < WW; x++)
                well[y][x] = well[y - 1][x];
        for (x = 0; x < WW; x++)
            well[0][x] = 0;
    }

    lines += nfull;
    score += pts[nfull] * (long)(level + 1);
    level = lines / 10;
    draw_well();
    draw_hud();
}

static void new_game(void)
{
    memset(well, 0, sizeof(well));
    score = 0;
    lines = 0;
    level = 0;
    next_p = rand() % 7;
    draw_screen();
}

/* Returns 1 to play again, 0 to leave. */
static int game_over(void)
{
    int w = 42, h = 7, x = (SCR_W - w) / 2, y = 9, k;
    char buf[40];
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " The wall is breached ", A_PANELHDR);
    sprintf(buf, "Score %ld   Lines %d   Level %d", score, lines, level);
    ui_puts(x + 3, y + 2, buf, A_PANEL);
    ui_puts(x + 3, y + 4, "R rebuild (play again)     Esc leave", A_PANEL);
    spk_bad();                          /* the wall comes down */
    for (;;) {
        k = wait_key();
        if (k == 'r' || k == 'R') return 1;
        if (k == KEY_ESC) return 0;
    }
}

static void do_pause(void)
{
    ui_puts(WX + 6, WY + 8, " PAUSED ", A_ITEMSEL);
    wait_key();
    ui_fill(WX + 6, WY + 8, 8, 1, ' ', A_DESKTOP);
    draw_well();
    draw_piece(0);
}

int main(void)
{
    int quit = 0;
    unsigned long deadline;

    srand((unsigned)ui_ticks());
    init_shapes();
    ui_init();
    spk_init();

restart:
    new_game();

    while (!quit) {
        /* Spawn. */
        cur_p = next_p;
        next_p = rand() % 7;
        cur_r = 0;
        cur_x = (WW - base_size[cur_p]) / 2;
        cur_y = 0;
        draw_next();
        if (collide(cur_p, cur_r, cur_x, cur_y)) {
            if (game_over())
                goto restart;
            break;
        }
        draw_piece(0);

        /* Fall until locked. */
        deadline = ui_ticks() + (unsigned long)fall_ticks();
        for (;;) {
            int locked = 0;

            while (ui_ticks() < deadline) {
                spk_poll();
                if (ui_ticks() + 100UL < deadline)
                    deadline = ui_ticks();      /* midnight wrap */
                if (!ui_keywaiting())
                    continue;
                switch (ui_getkey()) {
                case KEY_ESC:
                    quit = 1;
                    break;
                case 'p': case 'P':
                    do_pause();
                    deadline = ui_ticks() + (unsigned long)fall_ticks();
                    break;
                case 's': case 'S':
                    spk_mute(!spk_muted());
                    draw_sound_hint();
                    break;
                case KEY_LEFT:
                    draw_piece(1);
                    if (!collide(cur_p, cur_r, cur_x - 1, cur_y))
                        cur_x--;
                    draw_piece(0);
                    break;
                case KEY_RIGHT:
                    draw_piece(1);
                    if (!collide(cur_p, cur_r, cur_x + 1, cur_y))
                        cur_x++;
                    draw_piece(0);
                    break;
                case KEY_UP: {
                    int nr = (cur_r + 1) & 3;
                    draw_piece(1);
                    if (!collide(cur_p, nr, cur_x, cur_y)) {
                        cur_r = nr;
                    } else if (!collide(cur_p, nr, cur_x - 1, cur_y)) {
                        cur_x--; cur_r = nr;    /* wall kick left  */
                    } else if (!collide(cur_p, nr, cur_x + 1, cur_y)) {
                        cur_x++; cur_r = nr;    /* wall kick right */
                    }
                    draw_piece(0);
                    break;
                }
                case KEY_DOWN:
                    if (!collide(cur_p, cur_r, cur_x, cur_y + 1)) {
                        draw_piece(1);
                        cur_y++;
                        draw_piece(0);
                        score++;
                        deadline = ui_ticks()
                                 + (unsigned long)fall_ticks();
                    }
                    break;
                case KEY_SPACE:
                    draw_piece(1);
                    while (!collide(cur_p, cur_r, cur_x, cur_y + 1)) {
                        cur_y++;
                        score += 2;
                    }
                    draw_piece(0);
                    deadline = 0;               /* lock immediately */
                    break;
                default:
                    break;
                }
                if (quit)
                    break;
            }
            if (quit)
                break;

            /* Gravity step. */
            if (!collide(cur_p, cur_r, cur_x, cur_y + 1)) {
                draw_piece(1);
                cur_y++;
                draw_piece(0);
                deadline = ui_ticks() + (unsigned long)fall_ticks();
            } else {
                locked = 1;
            }
            if (locked) {
                lock_piece();
                clear_lines();
                draw_hud();
                break;
            }
        }
    }

    spk_off();
    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
