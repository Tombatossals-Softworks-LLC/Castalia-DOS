/* ===================================================================
 * REVERSI.C  -  CASTALIA REVERSI  (REVERSI.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Othello / Reversi on an 8x8 green board in 80x25 text mode.  You
 * play the white discs and move first; the machine plays black with a
 * depth-limited alpha-beta search over a positional weight table plus
 * mobility and disc-count terms.  Corners are prized, X-squares shunned.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os reversi.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.  16-bit int safe: every evaluation
 * accumulator stays well under +/-1000 and the minimax "infinity" bound
 * is 30000, so all arithmetic fits a signed 16-bit int.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/UI.H"
#include "../common/SPK.H"

/* Cell states. */
#define EMPTY 0
#define WHITE 1         /* human, moves first */
#define BLACK 2         /* the machine        */

/* Search / evaluation tuning (all products stay tiny -> 16-bit safe). */
#define DEPTH 4         /* plies of look-ahead */
#define INF   30000     /* our own "infinity" (no <limits.h>)          */
#define MOBW  5         /* weight of the mobility difference           */
#define DISCW 2         /* weight of the disc-count difference         */

/* Board placement on screen (cells 4 wide x 2 tall, grid lines shared). */
#define BX0 24
#define BY0 4

/* Right-hand scoreboard panel. */
#define PX 58
#define PY 4
#define PW 21
#define PH 16

/* Attributes used on the green field. */
#define A_BOARD UI_ATTR(C_LGRAY, C_GREEN)
#define A_GRID  UI_ATTR(C_DGRAY, C_GREEN)
#define A_WDISC UI_ATTR(C_WHITE, C_GREEN)
#define A_BDISC UI_ATTR(C_BLACK, C_GREEN)
#define A_MARK  UI_ATTR(C_YELLOW, C_GREEN)

static int board[8][8];
static int cur_r, cur_c;        /* human cursor */
static int show_hints;          /* draw legal-move markers this turn */

/* The eight bracketing directions. */
static const int DR[8] = { -1, -1, -1,  0, 0,  1, 1, 1 };
static const int DC[8] = { -1,  0,  1, -1, 1, -1, 0, 1 };

/* Positional weights.  Corners are gold; the diagonal "X-squares" next
 * to an empty corner are poison; edges are decent.  Kept small so the
 * summed evaluation never approaches the 16-bit ceiling. */
static const int WT[8][8] = {
    { 100, -10,  10,   5,   5,  10, -10, 100 },
    { -10, -40,   2,   2,   2,   2, -40, -10 },
    {  10,   2,   5,   1,   1,   5,   2,  10 },
    {   5,   2,   1,   1,   1,   1,   2,   5 },
    {   5,   2,   1,   1,   1,   1,   2,   5 },
    {  10,   2,   5,   1,   1,   5,   2,  10 },
    { -10, -40,   2,   2,   2,   2, -40, -10 },
    { 100, -10,  10,   5,   5,  10, -10, 100 }
};

#define ON(r, c) ((r) >= 0 && (r) < 8 && (c) >= 0 && (c) < 8)

/* --- Rules --------------------------------------------------------- */

/* Discs flipped for player p at (r,c) along one direction, 0 if none. */
static int line_flips(int bd[8][8], int r, int c, int p, int dr, int dc)
{
    int opp = 3 - p, rr = r + dr, cc = c + dc, cnt = 0;
    while (ON(rr, cc) && bd[rr][cc] == opp) {
        cnt++;
        rr += dr;
        cc += dc;
    }
    if (cnt > 0 && ON(rr, cc) && bd[rr][cc] == p)
        return cnt;
    return 0;
}

/* Total discs a move at (r,c) would flip (0 => illegal move). */
static int legal_move(int bd[8][8], int r, int c, int p)
{
    int d, tot = 0;
    if (bd[r][c] != EMPTY)
        return 0;
    for (d = 0; d < 8; d++)
        tot += line_flips(bd, r, c, p, DR[d], DC[d]);
    return tot;
}

static int count_moves(int bd[8][8], int p)
{
    int r, c, n = 0;
    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++)
            if (legal_move(bd, r, c, p) > 0)
                n++;
    return n;
}

static int count_discs(int bd[8][8], int p)
{
    int r, c, n = 0;
    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++)
            if (bd[r][c] == p)
                n++;
    return n;
}

/* Place p at (r,c) and flip every bracketed line (mutates bd). */
static void apply_move(int bd[8][8], int r, int c, int p)
{
    int d, i, cnt, rr, cc;
    bd[r][c] = p;
    for (d = 0; d < 8; d++) {
        cnt = line_flips(bd, r, c, p, DR[d], DC[d]);
        for (i = 1; i <= cnt; i++) {
            rr = r + DR[d] * i;
            cc = c + DC[d] * i;
            bd[rr][cc] = p;
        }
    }
}

/* --- Evaluation & search ------------------------------------------- */

/* Heuristic value from p's point of view.  Every term is bounded so the
 * result stays a few hundred in magnitude, safely inside a 16-bit int. */
static int eval(int bd[8][8], int p)
{
    int r, c, opp = 3 - p, pos = 0, mob, disc;
    for (r = 0; r < 8; r++) {
        for (c = 0; c < 8; c++) {
            if (bd[r][c] == p)
                pos += WT[r][c];
            else if (bd[r][c] == opp)
                pos -= WT[r][c];
        }
    }
    mob = MOBW * (count_moves(bd, p) - count_moves(bd, opp));
    disc = DISCW * (count_discs(bd, p) - count_discs(bd, opp));
    return pos + mob + disc;
}

/* Value of a finished game from p's view (winning is worth a lot). */
static int terminal_eval(int bd[8][8], int p)
{
    int d = count_discs(bd, p) - count_discs(bd, 3 - p);
    if (d > 0)
        return 10000 + d;
    if (d < 0)
        return -10000 + d;
    return 0;
}

/* Negamax with alpha-beta.  Passes keep the same depth (they are rare
 * and the board still fills, so recursion is bounded). */
static int negamax(int bd[8][8], int p, int depth, int alpha, int beta)
{
    int opp = 3 - p, r, c, best = -INF, any = 0, val;
    if (depth <= 0)
        return eval(bd, p);
    for (r = 0; r < 8; r++) {
        for (c = 0; c < 8; c++) {
            if (legal_move(bd, r, c, p) > 0) {
                int tmp[8][8];
                any = 1;
                memcpy(tmp, bd, sizeof(tmp));
                apply_move(tmp, r, c, p);
                val = -negamax(tmp, opp, depth - 1, -beta, -alpha);
                if (val > best)
                    best = val;
                if (best > alpha)
                    alpha = best;
                if (alpha >= beta)
                    return best;
            }
        }
    }
    if (!any) {
        if (count_moves(bd, opp) == 0)
            return terminal_eval(bd, p);
        return -negamax(bd, opp, depth, -beta, -alpha);
    }
    return best;
}

/* Pick the machine's best move into (*br,*bc). */
static void cpu_choose(int *br, int *bc)
{
    int r, c, best = -INF, alpha = -INF, val;
    *br = -1;
    *bc = -1;
    for (r = 0; r < 8; r++) {
        for (c = 0; c < 8; c++) {
            if (legal_move(board, r, c, BLACK) > 0) {
                int tmp[8][8];
                memcpy(tmp, board, sizeof(tmp));
                apply_move(tmp, r, c, BLACK);
                val = -negamax(tmp, WHITE, DEPTH - 1, -INF, -alpha);
                if (val > best) {
                    best = val;
                    *br = r;
                    *bc = c;
                }
                if (best > alpha)
                    alpha = best;
            }
        }
    }
}

/* --- Rendering ----------------------------------------------------- */

/* Box-drawing character for a grid intersection. */
static int grid_char(int l, int r, int u, int d)
{
    if (!u) {
        if (!l) return 0xDA;
        if (!r) return 0xBF;
        return 0xC2;
    }
    if (!d) {
        if (!l) return 0xC0;
        if (!r) return 0xD9;
        return 0xC1;
    }
    if (!l) return 0xC3;
    if (!r) return 0xB4;
    return 0xC5;
}

static void msg(const char *s)
{
    ui_fill(0, 22, SCR_W, 1, ' ', A_DESKTOP);
    if (s)
        ui_puts(BX0, 22, s, A_TITLE);
}

static void delay_ticks(unsigned long n)
{
    unsigned long t = ui_ticks() + n;
    while (ui_ticks() < t) {
        spk_poll();
        if (ui_ticks() + 40UL < t)      /* midnight wrap guard */
            break;
        ui_idle();
    }
    spk_poll();                     /* the search that follows never polls */
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

/* Draw one cell's contents (a disc, a hint, or bare field). */
static void draw_cell(int r, int c)
{
    int x = BX0 + c * 4 + 1, y = BY0 + r * 2 + 1, v = board[r][c];
    if (v == WHITE) {
        ui_fill(x, y, 3, 1, (char)0xDB, A_WDISC);
    } else if (v == BLACK) {
        ui_fill(x, y, 3, 1, (char)0xDB, A_BDISC);
    } else {
        ui_fill(x, y, 3, 1, ' ', A_BOARD);
        if (show_hints && legal_move(board, r, c, WHITE) > 0)
            ui_putc(x + 1, y, (char)0xFE, A_MARK);
    }
}

static void draw_cells(void)
{
    int r, c;
    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++)
            draw_cell(r, c);
}

/* Cursor is a pair of amber arrows hugging the cell; erasing restores
 * the shared vertical grid characters at that content row. */
static void draw_cursor(int r, int c, int on)
{
    int lx = BX0 + c * 4, rx = BX0 + c * 4 + 4, y = BY0 + r * 2 + 1;
    if (on) {
        ui_putc(lx, y, (char)0x10, A_MARK);
        ui_putc(rx, y, (char)0x11, A_MARK);
    } else {
        ui_putc(lx, y, (char)0xB3, A_GRID);
        ui_putc(rx, y, (char)0xB3, A_GRID);
    }
}

static void refresh_scores(int turn)
{
    int wc = count_discs(board, WHITE), bc = count_discs(board, BLACK);
    char buf[40];
    sprintf(buf, "Discs: %2d", wc);
    ui_puts(PX + 5, PY + 3, buf, A_PANEL);
    sprintf(buf, "Discs: %2d", bc);
    ui_puts(PX + 5, PY + 6, buf, A_PANEL);
    sprintf(buf, "Turn:  %s   ", (turn == WHITE) ? "You" : "CPU");
    ui_puts(PX + 2, PY + 8, buf, A_PANEL);
    sprintf(buf, "Sound: %-3s", spk_muted() ? "off" : "on");
    ui_puts(PX + 2, PY + 9, buf, A_PANEL);

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    sprintf(buf, " You:%d  CPU:%d  Turn:%s", wc, bc,
            (turn == WHITE) ? "You" : "CPU");
    ui_puts(0, SCR_H - 1, buf, A_STATUS);
    ui_puts(26, SCR_H - 1,
            "Arrows move  Enter place  R restart  S sound  Esc quit",
            A_STATUS);
}

static void draw_static(void)
{
    int i, j, x, y;

    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA REVERSI", A_TITLE);
    ui_puts(20, 0, "Othello vs. the machine", UI_ATTR(C_WHITE, C_BLUE));

    ui_fill(BX0, BY0, 33, 17, ' ', A_BOARD);
    for (j = 0; j <= 8; j++)
        for (i = 0; i <= 8; i++)
            ui_putc(BX0 + i * 4, BY0 + j * 2,
                    (char)grid_char(i > 0, i < 8, j > 0, j < 8), A_GRID);
    for (j = 0; j <= 8; j++) {
        y = BY0 + j * 2;
        for (i = 0; i < 8; i++) {
            x = BX0 + i * 4;
            ui_putc(x + 1, y, (char)0xC4, A_GRID);
            ui_putc(x + 2, y, (char)0xC4, A_GRID);
            ui_putc(x + 3, y, (char)0xC4, A_GRID);
        }
    }
    for (i = 0; i <= 8; i++) {
        x = BX0 + i * 4;
        for (j = 0; j < 8; j++)
            ui_putc(x, BY0 + j * 2 + 1, (char)0xB3, A_GRID);
    }
    for (i = 0; i < 8; i++)
        ui_putc(BX0 + i * 4 + 2, BY0 - 1, (char)('a' + i), A_TITLE);
    for (j = 0; j < 8; j++)
        ui_putc(BX0 - 2, BY0 + j * 2 + 1, (char)('1' + j), A_TITLE);

    ui_fill(PX, PY, PW, PH, ' ', A_PANEL);
    ui_box(PX, PY, PW, PH, A_PANEL);
    ui_fill(PX + 1, PY, PW - 2, 1, ' ', A_PANELHDR);
    ui_puts(PX + 2, PY, " Scoreboard", A_PANELHDR);
    ui_fill(PX + 2, PY + 2, 2, 1, (char)0xDB, UI_ATTR(C_WHITE, C_LGRAY));
    ui_puts(PX + 5, PY + 2, "You  (White)", A_PANEL);
    ui_fill(PX + 2, PY + 5, 2, 1, (char)0xDB, UI_ATTR(C_BLACK, C_LGRAY));
    ui_puts(PX + 5, PY + 5, "CPU  (Black)", A_PANEL);
    ui_putc(PX + 2, PY + 11, (char)0xFE, UI_ATTR(C_YELLOW, C_LGRAY));
    ui_puts(PX + 4, PY + 11, "= your move", A_PANEL);
    ui_puts(PX + 2, PY + 13, "Corners win games", A_PANELHDR);
}

/* Place a disc and animate the flips one by one. */
static void do_move_visual(int r, int c, int p)
{
    int d, i, cnt, rr, cc;
    /* White plays the upper voice, black the lower one; each flip is a
     * one-tick note and the flips are already tick-paced, so the run
     * comes out as a short arpeggio. */
    unsigned voice = (p == WHITE) ? 784U : 523U;
    board[r][c] = p;
    draw_cell(r, c);
    spk_blip();
    for (d = 0; d < 8; d++) {
        cnt = line_flips(board, r, c, p, DR[d], DC[d]);
        for (i = 1; i <= cnt; i++) {
            rr = r + DR[d] * i;
            cc = c + DC[d] * i;
            board[rr][cc] = p;
            draw_cell(rr, cc);
            spk_note(voice + (unsigned)(i * 40), 1);
            delay_ticks(1);
        }
    }
}

static void new_game(void)
{
    memset(board, 0, sizeof(board));
    board[3][3] = WHITE;
    board[4][4] = WHITE;
    board[3][4] = BLACK;
    board[4][3] = BLACK;
    cur_r = 2;
    cur_c = 4;
    show_hints = 0;
    draw_static();
    draw_cells();
    refresh_scores(WHITE);
    msg("Your move - amber squares are legal");
}

/* Returns 0 quit, 1 restart, 2 a move was made. */
static int human_turn(void)
{
    int k;
    show_hints = 1;
    draw_cells();
    draw_cursor(cur_r, cur_c, 1);
    refresh_scores(WHITE);
    msg("Your move - arrows to aim, Enter to place");
    for (;;) {
        k = wait_key();
        if (k == KEY_ESC)
            return 0;
        if (k == 'r' || k == 'R')
            return 1;
        if (k == 's' || k == 'S') {
            spk_mute(!spk_muted());
            refresh_scores(WHITE);
        }
        if (k == KEY_UP || k == KEY_DOWN ||
            k == KEY_LEFT || k == KEY_RIGHT) {
            int nr = cur_r, nc = cur_c;
            if (k == KEY_UP && nr > 0) nr--;
            else if (k == KEY_DOWN && nr < 7) nr++;
            else if (k == KEY_LEFT && nc > 0) nc--;
            else if (k == KEY_RIGHT && nc < 7) nc++;
            if (nr != cur_r || nc != cur_c) {
                draw_cursor(cur_r, cur_c, 0);
                cur_r = nr;
                cur_c = nc;
                draw_cursor(cur_r, cur_c, 1);
            }
        } else if (k == KEY_ENTER || k == KEY_SPACE) {
            if (legal_move(board, cur_r, cur_c, WHITE) > 0) {
                show_hints = 0;
                draw_cursor(cur_r, cur_c, 0);
                do_move_visual(cur_r, cur_c, WHITE);
                draw_cells();
                return 2;
            }
            spk_bad();
            msg("Illegal move - pick a marked square");
        }
    }
}

/* Returns 1 for a new game, 0 to leave. */
static int end_panel(void)
{
    int wc = count_discs(board, WHITE), bc = count_discs(board, BLACK);
    int w = 48, h = 9, x = (SCR_W - w) / 2, y = 8, k;
    const char *res;
    char buf[52];

    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " CASTALIA REVERSI - Game Over", A_PANELHDR);
    sprintf(buf, "Final: You(White) %d  vs  CPU(Black) %d", wc, bc);
    ui_puts(x + 3, y + 2, buf, A_PANEL);
    if (wc > bc)
        res = "You win!  Castalia salutes a sharp eye.";
    else if (bc > wc)
        res = "CASTALIA takes it.  Care to avenge?";
    else
        res = "A perfect draw - honours even.";
    ui_puts(x + 3, y + 4, res, A_PANEL);
    ui_puts(x + 3, y + 6, "R  new game            Esc  leave", A_PANEL);
    /* The board is full and a panel is up, so blocking is fair here. */
    if (wc > bc)
        spk_fanfare();
    else if (bc > wc)
        spk_tone(147, 4);
    else
        spk_tone(440, 4);
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
    int turn, act, wm, bm, br, bc;

    srand((unsigned)ui_ticks());
    ui_init();
    spk_init();
    new_game();
    turn = WHITE;

    for (;;) {
        refresh_scores(turn);
        wm = count_moves(board, WHITE);
        bm = count_moves(board, BLACK);

        if (wm == 0 && bm == 0) {           /* neither can move: over */
            act = end_panel();
            if (act == 1) {
                new_game();
                turn = WHITE;
                continue;
            }
            break;
        }
        if (count_moves(board, turn) == 0) { /* this side must pass */
            msg(turn == WHITE ? "You have no move - you pass"
                              : "CASTALIA has no move - it passes");
            delay_ticks(18);
            turn = 3 - turn;
            continue;
        }
        if (turn == WHITE) {
            act = human_turn();
            if (act == 0)
                break;
            if (act == 1) {
                new_game();
                turn = WHITE;
                continue;
            }
            turn = BLACK;
        } else {
            msg("CASTALIA is thinking...");
            refresh_scores(turn);
            cpu_choose(&br, &bc);
            delay_ticks(6);
            if (br >= 0) {
                do_move_visual(br, bc, BLACK);
                draw_cells();
                msg("CASTALIA plays.  Your move.");
            }
            turn = WHITE;
        }
        refresh_scores(turn);
    }

    spk_off();
    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
