/* ===================================================================
 * test_ui.c  -  host unit tests for the shared UI drawing primitives
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * UI.C is the one module every tool in the suite draws through, and it
 * was rewritten to write 16-bit CELL WORDS with the clipping hoisted out
 * of the per-cell path, instead of bounds-checking and storing two bytes
 * for every character on screen.  That is a worthwhile saving on a 386SX
 * with a 16-bit ISA video card - and exactly the sort of change that can
 * quietly shift a box edge by one column, or start drawing a string that
 * used to be clipped away.
 *
 * Nothing in CI can look at a real VGA text screen, so this test does
 * the next best thing: it aims the toolkit at an ordinary array (UI.C
 * takes its video base from UI_VRAM_BASE for exactly this reason), keeps
 * a REFERENCE COPY of the old per-cell implementations here, and asserts
 * that every primitive paints byte-for-byte the same 80x25 screen as the
 * code it replaced - including the awkward cases: negative coordinates,
 * rectangles hanging off every edge, strings longer than the screen,
 * degenerate boxes, and a randomised sweep of all of them together.
 *
 * The reference implementations below are the PRE-REWRITE code, copied
 * verbatim.  They are the specification; if a change to UI.C disagrees
 * with them, one of the two is wrong and this test says which call it
 * was.
 *
 * Needs only gcc.  Exit 0 = all green.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "UI.H"

/* --- the module under test, aimed at ordinary memory ------------------
 * Declared as words so the buffer is aligned for the cell-word stores;
 * UI.C is #included rather than linked so the test can reach its
 * statics and so UI_VRAM_BASE is in scope when it is compiled. */
static unsigned short vram[SCR_W * SCR_H];

#define UI_VRAM_BASE ((void *)vram)
#include "UI.C"

/* UI.C reaches the BIOS for the cursor and the keyboard.  There is no
 * BIOS here; none of the drawing primitives depend on these answering. */
int int86(int intno, union REGS *in, union REGS *out)
{
    (void)intno; (void)in; (void)out;
    return 0;
}
int int86x(int intno, union REGS *in, union REGS *out, struct SREGS *s)
{
    (void)intno; (void)in; (void)out; (void)s;
    return 0;
}
void segread(struct SREGS *s) { (void)s; }

/* --- reference screen: the pre-rewrite per-cell implementations ------ */

static unsigned char ref[SCR_W * SCR_H * 2];

static void ref_put_cell(int x, int y, char ch, unsigned char attr)
{
    unsigned offset;
    if (x < 0 || x >= SCR_W || y < 0 || y >= SCR_H)
        return;
    offset = (unsigned)((y * SCR_W + x) << 1);
    ref[offset]     = (unsigned char)ch;
    ref[offset + 1] = attr;
}

static void ref_cls(unsigned char attr)
{
    int i;
    unsigned offset = 0;
    for (i = 0; i < SCR_W * SCR_H; i++) {
        ref[offset]     = (unsigned char)' ';
        ref[offset + 1] = attr;
        offset += 2;
    }
}

static void ref_putlim(int x, int y, const char *s, int maxlen,
                       unsigned char attr)
{
    int i = 0;
    while (s[i] != '\0' && i < maxlen && (x + i) < SCR_W) {
        ref_put_cell(x + i, y, s[i], attr);
        i++;
    }
}

static void ref_puts(int x, int y, const char *s, unsigned char attr)
{
    ref_putlim(x, y, s, SCR_W, attr);
}

static void ref_fill(int x, int y, int w, int h, char ch, unsigned char attr)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            ref_put_cell(x + i, y + j, ch, attr);
}

static void ref_hline(int x, int y, int w, unsigned char attr)
{
    int i;
    for (i = 0; i < w; i++)
        ref_put_cell(x + i, y, (char)0xC4, attr);
}

static void ref_draw_box(int x, int y, int w, int h, unsigned char attr,
                         char tl, char tr, char bl, char br,
                         char hz, char vt)
{
    int i;
    if (w < 2 || h < 2)
        return;
    ref_put_cell(x, y, tl, attr);
    ref_put_cell(x + w - 1, y, tr, attr);
    ref_put_cell(x, y + h - 1, bl, attr);
    ref_put_cell(x + w - 1, y + h - 1, br, attr);
    for (i = 1; i < w - 1; i++) {
        ref_put_cell(x + i, y, hz, attr);
        ref_put_cell(x + i, y + h - 1, hz, attr);
    }
    for (i = 1; i < h - 1; i++) {
        ref_put_cell(x, y + i, vt, attr);
        ref_put_cell(x + w - 1, y + i, vt, attr);
    }
}

static void ref_box(int x, int y, int w, int h, unsigned char attr)
{
    ref_draw_box(x, y, w, h, attr, (char)0xDA, (char)0xBF, (char)0xC0,
                 (char)0xD9, (char)0xC4, (char)0xB3);
}

static void ref_dbox(int x, int y, int w, int h, unsigned char attr)
{
    ref_draw_box(x, y, w, h, attr, (char)0xC9, (char)0xBB, (char)0xC8,
                 (char)0xBC, (char)0xCD, (char)0xBA);
}

static void ref_center(int y, const char *s, unsigned char attr)
{
    int len = (int)strlen(s);
    int x = (SCR_W - len) / 2;
    if (x < 0)
        x = 0;
    ref_puts(x, y, s, attr);
}

static void ref_hbar(int x, int y, int w, int permille,
                     unsigned char attr, unsigned char dimattr)
{
    long total;
    int full, rem, i;

    if (permille < 0)    permille = 0;
    if (permille > 1000) permille = 1000;
    total = (long)w * (long)permille;
    full  = (int)(total / 1000L);
    rem   = (int)(total % 1000L);

    for (i = 0; i < full && i < w; i++)
        ref_put_cell(x + i, y, (char)0xDB, attr);
    if (full < w && rem > 0) {
        char c;
        if (rem >= 666)      c = (char)0xB2;
        else if (rem >= 333) c = (char)0xB1;
        else                 c = (char)0xB0;
        ref_put_cell(x + full, y, c, attr);
        full++;
    }
    for (i = full; i < w; i++)
        ref_put_cell(x + i, y, (char)0xFA, dimattr);
}

/* --- harness --------------------------------------------------------- */

static int failures = 0;
static int checks   = 0;

static void ck(int cond, const char *what)
{
    checks++;
    if (cond) {
        printf("  [  OK  ] %s\n", what);
    } else {
        printf("  [ FAIL ] %s\n", what);
        failures++;
    }
}

/* Both screens start from a known, DIFFERENT-from-blank pattern so a
 * primitive that draws nothing where it should draw something (or the
 * reverse) shows up instead of matching an accidentally equal blank. */
static void prime(void)
{
    int i;
    for (i = 0; i < SCR_W * SCR_H; i++) {
        vram[i]        = (unsigned short)(0x5A00u | (unsigned)('?' ));
        ref[i * 2]     = (unsigned char)'?';
        ref[i * 2 + 1] = 0x5A;
    }
}

/* Compare cell by cell (decoding, so the result does not depend on the
 * host's byte order) and report the first cell that differs.  'quiet'
 * suppresses the OK line for the randomised sweep, which would
 * otherwise bury the rest of the run in four hundred of them. */
static int screens_agree(const char *what, int quiet)
{
    int i;
    checks++;
    for (i = 0; i < SCR_W * SCR_H; i++) {
        unsigned char gch = (unsigned char)(vram[i] & 0x00FFu);
        unsigned char gat = (unsigned char)(vram[i] >> 8);
        if (gch != ref[i * 2] || gat != ref[i * 2 + 1]) {
            printf("  [ FAIL ] %s\n", what);
            printf("           first difference at col %d row %d:\n",
                   i % SCR_W, i / SCR_W);
            printf("           got  char %02X attr %02X\n", gch, gat);
            printf("           want char %02X attr %02X\n",
                   ref[i * 2], ref[i * 2 + 1]);
            failures++;
            return 0;
        }
    }
    if (!quiet)
        printf("  [  OK  ] %s\n", what);
    return 1;
}

static int same_screen(const char *what)
{
    return screens_agree(what, 0);
}

/* --- the cell layout the word stores assume -------------------------- */

static void test_cell_layout(void)
{
    const unsigned char *raw = (const unsigned char *)vram;
    unsigned short probe = 0x1234u;
    int little = (*(const unsigned char *)&probe == 0x34);

    printf("\n== a cell is one word: character low, attribute high ==\n");
    ui_init();
    prime();
    ui_putc(0, 0, 'A', 0x1E);
    ck((vram[0] & 0x00FFu) == (unsigned)'A', "character sits in the low byte");
    ck((vram[0] >> 8) == 0x1Eu, "attribute sits in the high byte");
    if (little) {
        ck(raw[0] == (unsigned char)'A' && raw[1] == 0x1E,
           "...so in memory it is 'char, attr' - VGA text-cell order");
    } else {
        printf("  [skip] host is big-endian; byte order check is DOS-only\n");
    }
}

/* --- per-primitive equivalence --------------------------------------- */

static void test_cls(void)
{
    printf("\n== ui_cls paints every cell ==\n");
    ui_init();
    prime();
    ui_cls(A_DESKTOP);
    ref_cls(A_DESKTOP);
    same_screen("ui_cls(A_DESKTOP)");

    prime();
    ui_cls(0x00);
    ref_cls(0x00);
    same_screen("ui_cls(0) - attribute zero still writes every cell");
}

static void test_fill(void)
{
    static const struct { int x, y, w, h; const char *what; } t[] = {
        {  0,  0, SCR_W, SCR_H, "whole screen"                      },
        {  3,  4,    20,     6, "an ordinary panel"                 },
        {  0,  0,     1,     1, "a single cell at the origin"       },
        { 79, 24,     1,     1, "a single cell at the far corner"   },
        { 70,  2,    20,     3, "hanging off the right edge"        },
        {  5, 20,    10,    12, "hanging off the bottom edge"       },
        { -5,  3,    20,     4, "hanging off the left edge"         },
        {  6, -4,    10,    10, "hanging off the top edge"          },
        {-10,-10,   100,   100, "bigger than the screen, all edges" },
        {  4,  4,     0,     5, "zero width"                        },
        {  4,  4,     7,     0, "zero height"                       },
        {  4,  4,    -3,     5, "negative width"                    },
        {  4,  4,     7,    -3, "negative height"                   },
        { 90,  4,    10,     5, "entirely off the right"            },
        {  4, 40,    10,     5, "entirely off the bottom"           },
        {-40,  4,    10,     5, "entirely off the left"             },
        {  4,-40,    10,     5, "entirely off the top"              }
    };
    int i, n = (int)(sizeof(t) / sizeof(t[0]));

    printf("\n== ui_fill clips the same rectangle as the per-cell code ==\n");
    for (i = 0; i < n; i++) {
        ui_init();
        prime();
        ui_fill(t[i].x, t[i].y, t[i].w, t[i].h, (char)0xB1, A_PANEL);
        ref_fill(t[i].x, t[i].y, t[i].w, t[i].h, (char)0xB1, A_PANEL);
        same_screen(t[i].what);
    }
}

static void test_putlim(void)
{
    static const char *lipsum =
        "The fortress holds against a very long line of text indeed, "
        "long enough to run past the right-hand edge of an 80 column "
        "screen and keep going.";
    static const struct { int x, y, maxlen; const char *s; const char *what; } t[] = {
        {  2,  3, SCR_W, "CASTALIA DOS",  "an ordinary string"                },
        {  0,  0, SCR_W, "at the origin", "at the origin"                     },
        {  2,  3,     4, "CASTALIA DOS",  "maxlen shorter than the string"    },
        {  2,  3,     0, "CASTALIA DOS",  "maxlen zero draws nothing"         },
        {  2,  3,    -1, "CASTALIA DOS",  "negative maxlen draws nothing"     },
        { 74,  3, SCR_W, "CASTALIA DOS",  "clipped by the right edge"         },
        { 79,  3, SCR_W, "CASTALIA DOS",  "one column left before the edge"   },
        { 80,  3, SCR_W, "CASTALIA DOS",  "starting past the right edge"      },
        { -4,  3, SCR_W, "CASTALIA DOS",  "starting left of the screen"       },
        {-11,  3, SCR_W, "CASTALIA DOS",  "all but one character off-left"    },
        {-40,  3, SCR_W, "CASTALIA DOS",  "entirely off-left"                 },
        { -4,  3,     6, "CASTALIA DOS",  "off-left, and maxlen bites too"    },
        { -4,  3,     3, "CASTALIA DOS",  "off-left, maxlen runs out first"   },
        {  2, -1, SCR_W, "CASTALIA DOS",  "above the screen"                  },
        {  2, 25, SCR_W, "CASTALIA DOS",  "below the screen"                  },
        {  2,  3, SCR_W, "",              "the empty string"                  },
        {  0, 12, SCR_W, NULL,            "a line longer than the screen"     }
    };
    int i, n = (int)(sizeof(t) / sizeof(t[0]));

    printf("\n== ui_putlim clips and counts exactly as before ==\n");
    for (i = 0; i < n; i++) {
        const char *s = t[i].s ? t[i].s : lipsum;
        ui_init();
        prime();
        ui_putlim(t[i].x, t[i].y, s, t[i].maxlen, A_TITLE);
        ref_putlim(t[i].x, t[i].y, s, t[i].maxlen, A_TITLE);
        same_screen(t[i].what);
    }

    ui_init();
    prime();
    ui_puts(9, 9, lipsum, A_ITEM);
    ref_puts(9, 9, lipsum, A_ITEM);
    same_screen("ui_puts caps at the screen width");

    ui_init();
    prime();
    ui_center(7, "the fortress holds", A_HINT);
    ref_center(7, "the fortress holds", A_HINT);
    same_screen("ui_center centres a short line");

    ui_init();
    prime();
    ui_center(7, lipsum, A_HINT);
    ref_center(7, lipsum, A_HINT);
    same_screen("ui_center pins an over-long line to column 0");
}

static void test_boxes(void)
{
    static const struct { int x, y, w, h; const char *what; } t[] = {
        {  0,  0, SCR_W, SCR_H, "a box round the whole screen"     },
        { 10,  5,    40,    12, "an ordinary dialog frame"         },
        {  0,  0,     2,     2, "the smallest box there is"        },
        { 10,  5,     1,     8, "width 1 - too thin to draw"       },
        { 10,  5,    20,     1, "height 1 - too thin to draw"      },
        { 10,  5,     0,     0, "zero by zero"                     },
        { 70,  3,    20,    10, "overhanging the right edge"       },
        {  5, 20,    30,    10, "overhanging the bottom edge"      },
        { -6,  4,    30,     8, "overhanging the left edge"        },
        {  6, -3,    30,     8, "overhanging the top edge"         },
        {-10,-10,   100,   100, "bigger than the screen"           }
    };
    int i, n = (int)(sizeof(t) / sizeof(t[0]));

    printf("\n== ui_box / ui_dbox draw the same frames ==\n");
    for (i = 0; i < n; i++) {
        ui_init();
        prime();
        ui_box(t[i].x, t[i].y, t[i].w, t[i].h, A_FRAME);
        ref_box(t[i].x, t[i].y, t[i].w, t[i].h, A_FRAME);
        same_screen(t[i].what);
    }
    for (i = 0; i < n; i++) {
        ui_init();
        prime();
        ui_dbox(t[i].x, t[i].y, t[i].w, t[i].h, A_FRAME);
        ref_dbox(t[i].x, t[i].y, t[i].w, t[i].h, A_FRAME);
        same_screen("double: it draws the same frame too");
    }

    printf("\n== ui_hline ==\n");
    ui_init(); prime();
    ui_hline(1, 14, SCR_W - 2, A_FRAME);
    ref_hline(1, 14, SCR_W - 2, A_FRAME);
    same_screen("a rule across the screen");

    ui_init(); prime();
    ui_hline(-5, 14, 200, A_FRAME);
    ref_hline(-5, 14, 200, A_FRAME);
    same_screen("a rule wider than the screen, starting off-left");

    ui_init(); prime();
    ui_hline(4, 30, 10, A_FRAME);
    ref_hline(4, 30, 10, A_FRAME);
    same_screen("a rule below the screen draws nothing");
}

static void test_hbar(void)
{
    static const int pm[] = { -100, 0, 1, 100, 332, 333, 500, 665, 666,
                              999, 1000, 1500 };
    int i, n = (int)(sizeof(pm) / sizeof(pm[0]));
    int w;

    printf("\n== ui_hbar: same blocks, shades and dots ==\n");
    for (i = 0; i < n; i++) {
        char what[64];
        ui_init();
        prime();
        ui_hbar(4, 12, 72, pm[i], A_TITLE, A_HINT);
        ref_hbar(4, 12, 72, pm[i], A_TITLE, A_HINT);
        sprintf(what, "%d permille of a 72 column bar", pm[i]);
        same_screen(what);
    }
    /* Narrow bars are where the partial-cell rounding shows. */
    for (w = 1; w <= 8; w++) {
        int p;
        for (p = 0; p <= 1000; p += 137) {
            ui_init();
            prime();
            ui_hbar(10, 6, w, p, A_TITLE, A_HINT);
            ref_hbar(10, 6, w, p, A_TITLE, A_HINT);
            if (!same_screen("narrow bar")) return;
        }
    }
    ui_init(); prime();
    ui_hbar(75, 6, 20, 400, A_TITLE, A_HINT);
    ref_hbar(75, 6, 20, 400, A_TITLE, A_HINT);
    same_screen("a bar that runs off the right edge");
}

static void test_putc(void)
{
    static const struct { int x, y; } t[] = {
        { 0, 0 }, { 79, 24 }, { 79, 0 }, { 0, 24 }, { 40, 12 },
        { -1, 5 }, { 80, 5 }, { 5, -1 }, { 5, 25 }, { -1, -1 }, { 999, 999 }
    };
    int i, n = (int)(sizeof(t) / sizeof(t[0]));

    printf("\n== ui_putc, in bounds and out ==\n");
    ui_init();
    prime();
    for (i = 0; i < n; i++) {
        ui_putc(t[i].x, t[i].y, (char)('a' + i), (unsigned char)(0x10 + i));
        ref_put_cell(t[i].x, t[i].y, (char)('a' + i),
                     (unsigned char)(0x10 + i));
    }
    same_screen("every cell landed where the old code put it");
}

/* --- randomised sweep -------------------------------------------------
 * Fixed seed: a failure here has to be reproducible, and this runs in
 * CI where nobody is watching it the first time. */
static void test_random(void)
{
    int round;

    printf("\n== 400 randomised screens of mixed drawing ==\n");
    srand(20260815u);
    for (round = 0; round < 400; round++) {
        int op, i;
        char what[64];

        ui_init();
        prime();
        for (i = 0; i < 12; i++) {
            int x = rand() % 120 - 20;
            int y = rand() % 40 - 8;
            int w = rand() % 50 - 5;
            int h = rand() % 20 - 3;
            unsigned char a = (unsigned char)(rand() & 0x7F);
            char c = (char)(rand() & 0xFF);

            op = rand() % 7;
            switch (op) {
            case 0:
                ui_fill(x, y, w, h, c, a);
                ref_fill(x, y, w, h, c, a);
                break;
            case 1:
                ui_box(x, y, w, h, a);
                ref_box(x, y, w, h, a);
                break;
            case 2:
                ui_dbox(x, y, w, h, a);
                ref_dbox(x, y, w, h, a);
                break;
            case 3:
                ui_putlim(x, y, "CASTALIA DOS - the fortress holds",
                          w, a);
                ref_putlim(x, y, "CASTALIA DOS - the fortress holds",
                           w, a);
                break;
            case 4:
                ui_hline(x, y, w, a);
                ref_hline(x, y, w, a);
                break;
            case 5: {
                /* One draw of rand() per pair: both sides must be fed
                 * the same permille or the comparison means nothing. */
                int p = rand() % 1200 - 100;
                ui_hbar(x, y, w, p, a, (unsigned char)(a ^ 0x08));
                ref_hbar(x, y, w, p, a, (unsigned char)(a ^ 0x08));
                break;
            }
            default:
                ui_putc(x, y, c, a);
                ref_put_cell(x, y, c, a);
                break;
            }
        }
        sprintf(what, "round %d", round);
        if (!screens_agree(what, 1))
            return;
    }
    printf("  [  OK  ] 400 rounds, 12 mixed calls each, all identical\n");
}

int main(void)
{
    printf("== CASTALIA UI primitives: word writes vs the per-cell "
           "reference ==\n");

    test_cell_layout();
    test_cls();
    test_fill();
    test_putlim();
    test_boxes();
    test_hbar();
    test_putc();
    test_random();

    printf("\n%d checks, %d failure(s)\n", checks, failures);
    if (failures) {
        printf("UI TESTS FAILED\n");
        return 1;
    }
    printf("ALL UI TESTS PASSED\n");
    return 0;
}
