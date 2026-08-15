/* ===================================================================
 * SOLITARE.C  -  CASTALIA SOLITAIRE  (SOLITARE.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Klondike solitaire (draw one) in 80x25 text mode.  A green baize
 * table with amber trim, a highlighted cursor that walks the thirteen
 * pile positions, pick-up / drop move handling and a rules engine that
 * enforces proper foundation and tableau building.  Turn based: the
 * loop blocks on the keyboard, so nothing spins the CPU.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os solitare.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/UI.H"
#include "../common/SPK.H"

/* --- Card model ----------------------------------------------------- */
/* rank 1..13  (A,2..10,J,Q,K)   suit 0=spade 1=club 2=heart 3=diamond */
typedef struct {
    unsigned char rank;
    unsigned char suit;
    unsigned char up;       /* 1 = face up */
} Card;

#define SPADE   0
#define CLUB    1
#define HEART   2
#define DIAMOND 3

/* --- Pile storage --------------------------------------------------- */
static Card stock[52];  static int nstock;
static Card waste[52];  static int nwaste;
static Card found[4][13]; static int nfound[4];
static Card tab[7][20];   static int ntab[7];

/* --- Cursor / play state -------------------------------------------- */
/* Pile positions 0..12: 0 stock, 1 waste, 2..5 foundations, 6..12 tableau. */
#define POS_STOCK 0
#define POS_WASTE 1
#define NPOS      13
#define is_fnd(p) ((p) >= 2 && (p) <= 5)
#define is_tab(p) ((p) >= 6 && (p) <= 12)

static int  cur;        /* current cursor position 0..12                */
static int  tsel;       /* topmost selected index in hovered tableau col */
static int  held;       /* 1 while a card/run is picked up               */
static int  hsrc;       /* source pile position of the held selection    */
static int  hidx;       /* source tableau top index of the held run      */
static long moves;      /* move counter for the score readout           */
static char msg[48];    /* transient status message                     */

/* --- Layout --------------------------------------------------------- */
#define CW      4       /* card cell width                              */
#define TOP_Y   3       /* row of stock/waste/foundation cards          */
#define STOCK_X 4
#define WASTE_X 10
#define TAB_Y   8       /* first row of the tableau cascade             */

static const int fnd_x[4] = { 48, 54, 60, 66 };
static const int tab_x[7] = { 6, 16, 26, 36, 46, 56, 66 };

/* Suit glyphs (CP437) and rank labels. */
static const char suit_ch[4] = { (char)0x06, (char)0x05, (char)0x03, (char)0x04 };
static const char *rank_str[14] = {
    "", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"
};

/* Attribute for the felt table field. */
#define A_TABLE UI_ATTR(C_WHITE, C_GREEN)
#define A_LABEL UI_ATTR(C_YELLOW, C_GREEN)

/* --- Small helpers -------------------------------------------------- */

static int is_red(int suit)
{
    return suit == HEART || suit == DIAMOND;
}

static void setmsg(const char *s)
{
    strncpy(msg, s, sizeof(msg) - 1);
    msg[sizeof(msg) - 1] = '\0';
}

/* Wait for a key while the speaker keeps its own time.  Klondike is turn
 * based, so a bare ui_getkey() would hold the last note until the player
 * moved again; spk_poll() retires it on schedule instead. */
static int wait_key(void)
{
    while (!ui_keywaiting()) {
        spk_poll();
        ui_idle();
    }
    spk_poll();
    return ui_getkey();
}

static int total_home(void)
{
    int f, s = 0;
    for (f = 0; f < 4; f++)
        s += nfound[f];
    return s;
}

/* First (topmost) face-up index of a tableau column, or ntab if none. */
static int first_faceup(int c)
{
    int j;
    for (j = 0; j < ntab[c]; j++)
        if (tab[c][j].up)
            return j;
    return ntab[c];
}

/* --- Rules engine --------------------------------------------------- */

/* May card c be placed on foundation slot f?  (up by suit, A..K) */
static int can_to_found(Card c, int f)
{
    Card t;
    if (nfound[f] == 0)
        return c.rank == 1;
    t = found[f][nfound[f] - 1];
    return t.suit == c.suit && t.rank + 1 == c.rank;
}

/* May card c head a run dropped on tableau column t?  (down, alt colour) */
static int can_stack_tab(Card c, int t)
{
    Card top;
    if (ntab[t] == 0)
        return c.rank == 13;            /* only a King on an empty column */
    top = tab[t][ntab[t] - 1];
    return top.rank == c.rank + 1 &&
           (is_red(top.suit) != is_red(c.suit));
}

/* Which foundation should this card go to?  -1 if none can take it. */
static int found_index_for(Card c)
{
    int f, empty = -1;
    for (f = 0; f < 4; f++) {
        if (nfound[f] > 0) {
            if (found[f][0].suit == c.suit)
                return f;
        } else if (empty < 0) {
            empty = f;
        }
    }
    if (c.rank == 1)
        return empty;                   /* an Ace starts a new foundation */
    return -1;
}

/* Top rank currently on the foundation holding 'suit', 0 if not started. */
static int foundrank_suit(int suit)
{
    int f;
    for (f = 0; f < 4; f++)
        if (nfound[f] > 0 && found[f][0].suit == suit)
            return found[f][nfound[f] - 1].rank;
    return 0;
}

/* Safe-autoplay test: never strand a card a tableau might still need. */
static int is_safe_up(Card c)
{
    int r = c.rank, o1, o2;
    if (r <= 2)
        return 1;
    if (is_red(c.suit)) {               /* opposite colour = black suits */
        o1 = foundrank_suit(SPADE);
        o2 = foundrank_suit(CLUB);
    } else {                            /* opposite colour = red suits   */
        o1 = foundrank_suit(HEART);
        o2 = foundrank_suit(DIAMOND);
    }
    return o1 >= r - 1 && o2 >= r - 1;
}

/* --- Move execution ------------------------------------------------- */

/* Turn one card stock->waste, or recycle the whole waste back to stock. */
static void draw_stock(void)
{
    if (nstock > 0) {
        Card cc = stock[--nstock];
        cc.up = 1;
        waste[nwaste++] = cc;
        moves++;
        spk_blip();
    } else if (nwaste > 0) {
        while (nwaste > 0) {            /* reverse waste back into stock */
            Card cc = waste[--nwaste];
            cc.up = 0;
            stock[nstock++] = cc;
        }
        moves++;
        spk_note(392, 2);               /* the deck is turned over */
    }
}

/* Remove the held selection from its source pile, flipping a newly
 * exposed tableau card face-up. */
static void remove_from_source(void)
{
    if (hsrc == POS_WASTE) {
        nwaste--;
    } else if (is_fnd(hsrc)) {
        nfound[hsrc - 2]--;
    } else {
        int c = hsrc - 6;
        ntab[c] = hidx;
        if (ntab[c] > 0 && !tab[c][ntab[c] - 1].up)
            tab[c][ntab[c] - 1].up = 1;
    }
}

/* Pick up the card/run under the cursor into the held selection. */
static void pickup(void)
{
    if (cur == POS_WASTE) {
        if (nwaste == 0) { spk_bad(); setmsg("Waste is empty"); return; }
        held = 1; hsrc = cur;
    } else if (is_fnd(cur)) {
        int f = cur - 2;
        if (nfound[f] == 0) {
            spk_bad(); setmsg("Foundation is empty"); return;
        }
        held = 1; hsrc = cur;
    } else if (is_tab(cur)) {
        int c = cur - 6;
        if (ntab[c] == 0) { spk_bad(); setmsg("Column is empty"); return; }
        if (!tab[c][tsel].up) {
            spk_bad(); setmsg("That card is face down"); return;
        }
        held = 1; hsrc = cur; hidx = tsel;
    }
    if (held) {
        spk_blip();
        setmsg("Picked up - choose a destination");
    }
}

/* Attempt to drop the held selection onto pile 'dest'. */
static void drop(int dest)
{
    Card mv;
    int runlen;

    if (dest == hsrc) {                 /* dropped back = cancel         */
        held = 0;
        spk_blip();
        setmsg("Cancelled");
        return;
    }

    if (hsrc == POS_WASTE) {
        mv = waste[nwaste - 1];
        runlen = 1;
    } else if (is_fnd(hsrc)) {
        mv = found[hsrc - 2][nfound[hsrc - 2] - 1];
        runlen = 1;
    } else {
        int c = hsrc - 6;
        mv = tab[c][hidx];
        runlen = ntab[c] - hidx;
    }

    if (is_fnd(dest)) {
        int f = dest - 2;
        if (runlen != 1) {
            spk_bad();
            setmsg("Only one card to a foundation");
            held = 0; return;
        }
        if (!can_to_found(mv, f)) {
            spk_bad();
            setmsg("That does not fit the foundation");
            held = 0; return;
        }
        found[f][nfound[f]++] = mv;
        remove_from_source();
        moves++;
        spk_ok();                       /* another card is home */
    } else if (is_tab(dest)) {
        int t = dest - 6;
        if (!can_stack_tab(mv, t)) {
            spk_bad();
            setmsg("Cannot stack there");
            held = 0; return;
        }
        if (is_tab(hsrc)) {
            int c = hsrc - 6, j;
            for (j = hidx; j < ntab[c]; j++)
                tab[t][ntab[t]++] = tab[c][j];
        } else {
            tab[t][ntab[t]++] = mv;
        }
        remove_from_source();
        moves++;
        spk_blip();
    } else {
        spk_bad();
        setmsg("Not a place to drop");
        held = 0;
        return;
    }

    held = 0;
    setmsg("");
}

/* Send every card that can safely go home to a foundation. */
static void autoplay(void)
{
    int moved = 1, c, f;
    int before = total_home();
    while (moved) {
        moved = 0;
        if (nwaste > 0) {
            Card w = waste[nwaste - 1];
            f = found_index_for(w);
            if (f >= 0 && can_to_found(w, f) && is_safe_up(w)) {
                found[f][nfound[f]++] = w;
                nwaste--;
                moves++;
                moved = 1;
                continue;
            }
        }
        for (c = 0; c < 7; c++) {
            if (ntab[c] > 0) {
                Card t = tab[c][ntab[c] - 1];
                if (t.up) {
                    f = found_index_for(t);
                    if (f >= 0 && can_to_found(t, f) && is_safe_up(t)) {
                        found[f][nfound[f]++] = t;
                        ntab[c]--;
                        if (ntab[c] > 0 && !tab[c][ntab[c] - 1].up)
                            tab[c][ntab[c] - 1].up = 1;
                        moves++;
                        moved = 1;
                        break;
                    }
                }
            }
        }
    }
    /* One note for the whole run, not one per card. */
    if (total_home() != before)
        spk_ok();
}

/* --- Rendering ------------------------------------------------------ */

static void draw_card(int x, int y, Card c, int hl)
{
    int fg = is_red(c.suit) ? C_RED : C_BLACK;
    int bg = (hl == 1) ? C_YELLOW : (hl == 2) ? C_CYAN : C_WHITE;
    unsigned char a = UI_ATTR(fg, bg);
    char lbl[8];
    if (y < 2)  y = 2;
    if (y > 23) y = 23;
    ui_fill(x, y, CW, 1, ' ', a);
    sprintf(lbl, "%s%c", rank_str[c.rank], suit_ch[c.suit]);
    ui_puts(x + 1, y, lbl, a);
}

static void draw_back(int x, int y, int hl)
{
    unsigned char a = (hl == 1) ? UI_ATTR(C_YELLOW, C_BLUE)
                                : UI_ATTR(C_LBLUE, C_BLUE);
    if (y > 23) y = 23;
    ui_fill(x, y, CW, 1, (char)0xB1, a);
}

static void draw_empty(int x, int y, int hl, char marker)
{
    unsigned char a = (hl == 1) ? UI_ATTR(C_BLACK, C_YELLOW)
                                : UI_ATTR(C_DGRAY, C_GREEN);
    if (y > 23) y = 23;
    ui_fill(x, y, CW, 1, ' ', a);
    ui_putc(x + 1, y, marker, a);
}

static void render(void)
{
    int f, c, j;
    char line[80];

    /* Title bar. */
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA SOLITAIRE", A_TITLE);
    ui_puts(SCR_W - 16, 0, "Klondike \x06\x05\x03\x04", A_TITLE);

    /* Score / message line and the green felt field. */
    ui_fill(0, 1, SCR_W, 1, ' ', A_LABEL);
    ui_fill(0, 2, SCR_W, SCR_H - 3, ' ', A_TABLE);
    sprintf(line, "Moves %ld   Home %d of 52   Sound %s",
            moves, total_home(), spk_muted() ? "off" : "on");
    ui_puts(2, 1, line, A_LABEL);
    if (msg[0])
        ui_puts(40, 1, msg, UI_ATTR(C_WHITE, C_RED));

    /* Labels. */
    ui_puts(STOCK_X, TOP_Y - 1, "Stock", A_LABEL);
    ui_puts(WASTE_X, TOP_Y - 1, "Waste", A_LABEL);
    ui_puts(fnd_x[0], TOP_Y - 1, "Foundations", A_LABEL);
    for (c = 0; c < 7; c++) {
        char num[2];
        num[0] = (char)('1' + c);
        num[1] = '\0';
        ui_puts(tab_x[c] + 1, TAB_Y - 1, num, A_LABEL);
    }

    /* Stock, waste, foundations. */
    if (nstock > 0)
        draw_back(STOCK_X, TOP_Y, 0);
    else
        draw_empty(STOCK_X, TOP_Y, 0, (char)0x12);   /* recycle glyph */
    if (nwaste > 0)
        draw_card(WASTE_X, TOP_Y, waste[nwaste - 1], 0);
    else
        draw_empty(WASTE_X, TOP_Y, 0, (char)0xB0);
    for (f = 0; f < 4; f++) {
        if (nfound[f] > 0)
            draw_card(fnd_x[f], TOP_Y, found[f][nfound[f] - 1], 0);
        else
            draw_empty(fnd_x[f], TOP_Y, 0, (char)0xB0);
    }

    /* Tableau columns (one card per row, all ranks readable). */
    for (c = 0; c < 7; c++) {
        if (ntab[c] == 0) {
            draw_empty(tab_x[c], TAB_Y, 0, (char)0xB0);
        } else {
            for (j = 0; j < ntab[c]; j++) {
                int y = TAB_Y + j;
                if (y > 23) y = 23;
                if (tab[c][j].up)
                    draw_card(tab_x[c], y, tab[c][j], 0);
                else
                    draw_back(tab_x[c], y, 0);
            }
        }
    }

    /* Held selection overlay (cyan). */
    if (held) {
        if (hsrc == POS_WASTE) {
            draw_card(WASTE_X, TOP_Y, waste[nwaste - 1], 2);
        } else if (is_fnd(hsrc)) {
            int fi = hsrc - 2;
            draw_card(fnd_x[fi], TOP_Y, found[fi][nfound[fi] - 1], 2);
        } else if (is_tab(hsrc)) {
            int cc = hsrc - 6;
            for (j = hidx; j < ntab[cc]; j++) {
                int y = TAB_Y + j;
                if (y > 23) y = 23;
                draw_card(tab_x[cc], y, tab[cc][j], 2);
            }
        }
    }

    /* Cursor overlay (amber). */
    if (cur == POS_STOCK) {
        if (nstock > 0) draw_back(STOCK_X, TOP_Y, 1);
        else            draw_empty(STOCK_X, TOP_Y, 1, (char)0x12);
    } else if (cur == POS_WASTE) {
        if (nwaste > 0) draw_card(WASTE_X, TOP_Y, waste[nwaste - 1], 1);
        else            draw_empty(WASTE_X, TOP_Y, 1, (char)0xB0);
    } else if (is_fnd(cur)) {
        int fi = cur - 2;
        if (nfound[fi] > 0) draw_card(fnd_x[fi], TOP_Y, found[fi][nfound[fi] - 1], 1);
        else                draw_empty(fnd_x[fi], TOP_Y, 1, (char)0xB0);
    } else {
        int cc = cur - 6;
        if (ntab[cc] == 0) {
            draw_empty(tab_x[cc], TAB_Y, 1, (char)0xB0);
        } else {
            int s = held ? ntab[cc] - 1 : tsel;
            if (s < 0) s = 0;
            for (j = s; j < ntab[cc]; j++) {
                int y = TAB_Y + j;
                if (y > 23) y = 23;
                if (tab[cc][j].up) draw_card(tab_x[cc], y, tab[cc][j], 1);
                else               draw_back(tab_x[cc], y, 1);
            }
        }
    }

    /* Bottom status bar. */
    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(0, SCR_H - 1,
        "\x1b\x1a Move  \x18\x19 Depth  Enter Pick/Drop  Space Draw"
        "  A Auto  R New  S Sound  Esc Quit", A_STATUS);
}

/* --- Game setup and cursor sync ------------------------------------- */

static void deal(void)
{
    Card deck[52];
    int n = 0, r, s, i, cidx, k, col;

    for (s = 0; s < 4; s++)
        for (r = 1; r <= 13; r++) {
            deck[n].rank = (unsigned char)r;
            deck[n].suit = (unsigned char)s;
            deck[n].up = 0;
            n++;
        }
    for (i = 51; i > 0; i--) {           /* Fisher-Yates shuffle */
        int jj = rand() % (i + 1);
        Card tmp = deck[i];
        deck[i] = deck[jj];
        deck[jj] = tmp;
    }

    nstock = 0; nwaste = 0;
    for (i = 0; i < 4; i++) nfound[i] = 0;
    for (i = 0; i < 7; i++) ntab[i] = 0;

    cidx = 0;
    for (col = 0; col < 7; col++)
        for (k = 0; k <= col; k++) {
            Card cc = deck[cidx++];
            cc.up = (unsigned char)(k == col ? 1 : 0);
            tab[col][ntab[col]++] = cc;
        }
    while (cidx < 52) {
        Card cc = deck[cidx++];
        cc.up = 0;
        stock[nstock++] = cc;
    }

    cur = 0; tsel = 0; held = 0; moves = 0;
    msg[0] = '\0';
}

/* Reset the tableau depth selector when the cursor lands on a column. */
static void sync_tsel(void)
{
    if (is_tab(cur)) {
        int c = cur - 6;
        if (ntab[c] == 0) {
            tsel = 0;
        } else {
            int fu = first_faceup(c);
            tsel = ntab[c] - 1;
            if (tsel < fu) tsel = fu;
        }
    }
}

static int win_panel(void)
{
    int w = 44, h = 9, x = (SCR_W - w) / 2, y = 8, k;
    char b[48];

    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " CASTALIA SOLITAIRE ", A_PANELHDR);
    ui_center(y + 2, "Y O U   W I N !", UI_ATTR(C_YELLOW, C_LGRAY));
    sprintf(b, "All 52 cards home in %ld moves.", moves);
    ui_center(y + 4, b, A_PANEL);
    ui_center(y + 6, "R  New game        Esc  Quit", A_PANEL);
    spk_fanfare();                  /* blocking: the game is already won */
    for (;;) {
        k = wait_key();
        if (k == 'r' || k == 'R') return 1;
        if (k == KEY_ESC) return 0;
    }
}

/* --- Main loop ------------------------------------------------------ */

int main(void)
{
    int k, playing = 1;

    srand((unsigned)ui_ticks());
    ui_init();
    spk_init();

    while (playing) {
        deal();
        render();
        for (;;) {
            k = wait_key();

            if (k == KEY_ESC) {
                playing = 0;
                break;
            } else if (k == 'r' || k == 'R') {
                break;                          /* redeal a new game */
            } else if (k == 's' || k == 'S') {
                spk_mute(!spk_muted());
                setmsg(spk_muted() ? "Sound off" : "Sound on");
            } else if ((k == 'a' || k == 'A') && !held) {
                autoplay();
                setmsg("");
            } else if (k == KEY_LEFT) {
                cur = (cur + NPOS - 1) % NPOS;
                sync_tsel();
                if (!held) setmsg("");
            } else if (k == KEY_RIGHT) {
                cur = (cur + 1) % NPOS;
                sync_tsel();
                if (!held) setmsg("");
            } else if (k == KEY_UP) {
                if (is_tab(cur)) {
                    int c = cur - 6, fu = first_faceup(c);
                    if (ntab[c] > 0 && tsel > fu) tsel--;
                }
            } else if (k == KEY_DOWN) {
                if (is_tab(cur)) {
                    int c = cur - 6;
                    if (ntab[c] > 0 && tsel < ntab[c] - 1) tsel++;
                }
            } else if (k == KEY_SPACE) {
                if (!held && cur == POS_STOCK) {
                    draw_stock();
                    setmsg("");
                }
            } else if (k == KEY_ENTER) {
                if (cur == POS_STOCK && !held) {
                    draw_stock();
                    setmsg("");
                } else if (!held) {
                    pickup();
                } else {
                    drop(cur);
                }
            }

            if (total_home() == 52) {
                if (win_panel()) break;         /* new game */
                playing = 0;
                break;
            }
            render();
        }
    }

    spk_off();
    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
