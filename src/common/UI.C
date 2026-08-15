/* ===================================================================
 * UI.C  -  Text-mode user-interface helpers for CASTALIA DOS tools
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * See UI.H for the interface.  Uses direct writes to colour text video
 * memory at B800:0000 (VGA), the BIOS video service (INT 10h) for the
 * cursor, and the BIOS keyboard service (INT 16h) for input.  C89.
 *
 * HOW THE CELLS ARE WRITTEN
 * -------------------------
 * A text cell is a character byte followed by an attribute byte, so a
 * cell is exactly one little-endian 16-bit word and the whole screen is
 * 2000 words.  Everything here writes WORDS, never byte pairs: on a
 * 386SX with a 16-bit ISA video card that halves the bus cycles for
 * every fill, string and box the suite draws, and there is no primitive
 * that ever wants to touch a character without its attribute.
 *
 * Clipping is done ONCE per call, on the rectangle or the run, rather
 * than per cell.  ui_putc() keeps the per-cell test because a single
 * cell is all it draws.
 * =================================================================== */

#include <dos.h>
#include <string.h>
#include "UI.H"

/* MK_FP is in <dos.h> on Turbo C and Open Watcom, but define a fallback
 * so the file builds even where the macro is absent. */
#ifndef MK_FP
#define MK_FP(seg, ofs) \
    ((void far *)(((unsigned long)(seg) << 16) | (unsigned)(ofs)))
#endif

/* CP437 line-drawing characters. */
#define CH_SH   (char)0xC4   /* single horizontal */
#define CH_SV   (char)0xB3   /* single vertical   */
#define CH_STL  (char)0xDA   /* single top-left   */
#define CH_STR  (char)0xBF   /* single top-right  */
#define CH_SBL  (char)0xC0   /* single bot-left   */
#define CH_SBR  (char)0xD9   /* single bot-right  */
#define CH_DH   (char)0xCD   /* double horizontal */
#define CH_DV   (char)0xBA   /* double vertical   */
#define CH_DTL  (char)0xC9   /* double top-left   */
#define CH_DTR  (char)0xBB   /* double top-right  */
#define CH_DBL  (char)0xC8   /* double bot-left   */
#define CH_DBR  (char)0xBC   /* double bot-right  */

/* Base of colour text video RAM.  A macro so the host unit test can aim
 * the toolkit at an ordinary array and check what it drew; nothing but
 * tests/unit/test_ui.c ever defines it. */
#ifndef UI_VRAM_BASE
#define UI_VRAM_BASE MK_FP(0xB800, 0x0000)
#endif

/* The screen as 2000 cell words.  Set in ui_init(). */
static unsigned short far *ui_cells = (unsigned short far *)0;

/* Character byte low, attribute byte high - the layout of a text cell. */
#define UI_CELL(ch, attr) \
    ((unsigned short)((unsigned short)(unsigned char)(ch) \
                      | ((unsigned short)(attr) << 8)))

/* Address of a cell.  Callers clip first: y*80+x tops out at 1999, so
 * the index stays inside an unsigned int on a 16-bit build. */
#define UI_AT(x, y)  (ui_cells + (unsigned)(y) * SCR_W + (unsigned)(x))

/* --- Lifecycle ------------------------------------------------------ */

void ui_init(void)
{
    ui_cells = (unsigned short far *)UI_VRAM_BASE;
    ui_cursor(0);
}

void ui_done(void)
{
    ui_gotoxy(0, SCR_H - 1);
    ui_cursor(1);
}

/* --- Low-level cell write ------------------------------------------- */

static void put_cell(int x, int y, char ch, unsigned char attr)
{
    if (x < 0 || x >= SCR_W || y < 0 || y >= SCR_H)
        return;
    *UI_AT(x, y) = UI_CELL(ch, attr);
}

/* --- Drawing -------------------------------------------------------- */

void ui_putc(int x, int y, char ch, unsigned char attr)
{
    put_cell(x, y, ch, attr);
}

void ui_fill(int x, int y, int w, int h, char ch, unsigned char attr)
{
    unsigned short far *p;
    unsigned short cell;
    int i, j;

    /* Clip the rectangle to the screen once, then write it blind. */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (w > SCR_W - x) w = SCR_W - x;
    if (h > SCR_H - y) h = SCR_H - y;
    if (w <= 0 || h <= 0 || x >= SCR_W || y >= SCR_H)
        return;

    cell = UI_CELL(ch, attr);
    p    = UI_AT(x, y);
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++)
            p[i] = cell;
        p += SCR_W;
    }
}

void ui_cls(unsigned char attr)
{
    ui_fill(0, 0, SCR_W, SCR_H, ' ', attr);
}

void ui_putlim(int x, int y, const char *s, int maxlen, unsigned char attr)
{
    unsigned short far *p;
    unsigned short hi;
    int i = 0;

    if (y < 0 || y >= SCR_H || x >= SCR_W || maxlen <= 0)
        return;
    /* Characters that fall off the left edge are consumed, not drawn -
     * they still count against maxlen, exactly as the per-cell version
     * did when put_cell() dropped them. */
    while (x < 0 && i < maxlen && s[i] != '\0') {
        i++;
        x++;
    }
    if (x < 0)
        return;

    hi = (unsigned short)((unsigned short)attr << 8);
    p  = UI_AT(x, y);
    while (i < maxlen && x < SCR_W && s[i] != '\0') {
        *p++ = (unsigned short)(hi | (unsigned short)(unsigned char)s[i]);
        i++;
        x++;
    }
}

void ui_puts(int x, int y, const char *s, unsigned char attr)
{
    ui_putlim(x, y, s, SCR_W, attr);
}

void ui_hline(int x, int y, int w, unsigned char attr)
{
    ui_fill(x, y, w, 1, CH_SH, attr);
}

static void draw_box(int x, int y, int w, int h, unsigned char attr,
                     char tl, char tr, char bl, char br, char hz, char vt)
{
    if (w < 2 || h < 2)
        return;
    put_cell(x, y, tl, attr);
    put_cell(x + w - 1, y, tr, attr);
    put_cell(x, y + h - 1, bl, attr);
    put_cell(x + w - 1, y + h - 1, br, attr);
    ui_fill(x + 1, y,         w - 2, 1,     hz, attr);
    ui_fill(x + 1, y + h - 1, w - 2, 1,     hz, attr);
    ui_fill(x,     y + 1,     1,     h - 2, vt, attr);
    ui_fill(x + w - 1, y + 1, 1,     h - 2, vt, attr);
}

void ui_box(int x, int y, int w, int h, unsigned char attr)
{
    draw_box(x, y, w, h, attr,
             CH_STL, CH_STR, CH_SBL, CH_SBR, CH_SH, CH_SV);
}

void ui_dbox(int x, int y, int w, int h, unsigned char attr)
{
    draw_box(x, y, w, h, attr,
             CH_DTL, CH_DTR, CH_DBL, CH_DBR, CH_DH, CH_DV);
}

void ui_center(int y, const char *s, unsigned char attr)
{
    int len = (int)strlen(s);
    int x = (SCR_W - len) / 2;
    if (x < 0)
        x = 0;
    ui_puts(x, y, s, attr);
}

/* --- Cursor / input (BIOS) ------------------------------------------ */

void ui_gotoxy(int x, int y)
{
    union REGS r;
    r.h.ah = 0x02;
    r.h.bh = 0x00;
    r.h.dh = (unsigned char)y;
    r.h.dl = (unsigned char)x;
    int86(0x10, &r, &r);
}

void ui_cursor(int on)
{
    union REGS r;
    r.h.ah = 0x01;
    if (on) {
        r.h.ch = 0x06;      /* normal underline cursor */
        r.h.cl = 0x07;
    } else {
        r.h.ch = 0x20;      /* bit 5 set = cursor off   */
        r.h.cl = 0x00;
    }
    int86(0x10, &r, &r);
}

int ui_getkey(void)
{
    union REGS r;
    r.h.ah = 0x00;
    int86(0x16, &r, &r);
    if (r.h.al == 0x00 || r.h.al == (unsigned char)0xE0)
        return 0x100 | r.h.ah;      /* extended key */
    return r.h.al;
}

int ui_keywaiting(void)
{
    /* BIOS keyboard buffer head/tail pointers in the BIOS data area.
     * head != tail means a keystroke is waiting.  Fully portable (no
     * flags-register access needed, unlike INT 16h AH=01h). */
    unsigned short far *head = (unsigned short far *)MK_FP(0x0040, 0x001A);
    unsigned short far *tail = (unsigned short far *)MK_FP(0x0040, 0x001C);
    return (*head != *tail);
}

/* --- Timing ---------------------------------------------------------- */

unsigned long ui_ticks(void)
{
    unsigned long far *t = (unsigned long far *)MK_FP(0x0040, 0x006C);
    return *t;
}

void ui_idle(void)
{
    union REGS r;
    r.x.ax = 0;
    int86(0x28, &r, &r);            /* DOS idle: let TSRs run / host throttle */
}

/* --- Widgets ---------------------------------------------------------- */

void ui_hbar(int x, int y, int w, int permille,
             unsigned char attr, unsigned char dimattr)
{
    long total;
    int full, rem;

    if (permille < 0)    permille = 0;
    if (permille > 1000) permille = 1000;
    total = (long)w * (long)permille;
    full  = (int)(total / 1000L);
    rem   = (int)(total % 1000L);

    if (full > w)
        full = w;
    ui_fill(x, y, full, 1, (char)0xDB, attr);
    if (full < w && rem > 0) {
        char c;
        if (rem >= 666)      c = (char)0xB2;
        else if (rem >= 333) c = (char)0xB1;
        else                 c = (char)0xB0;
        ui_putc(x + full, y, c, attr);
        full++;
    }
    ui_fill(x + full, y, w - full, 1, (char)0xFA, dimattr);
}

#define UI_ED_ATTR UI_ATTR(C_WHITE, C_BLACK)

int ui_editline(char *buf, int maxlen, int x, int y, int w)
{
    int len = (int)strlen(buf);
    int cur = len, off = 0, k;

    ui_cursor(1);
    for (;;) {
        if (cur < off)      off = cur;
        if (cur >= off + w) off = cur - w + 1;
        ui_fill(x, y, w, 1, ' ', UI_ED_ATTR);
        /* buf is NUL-terminated at len, so putlim stops at the same place
         * the old per-character loop did. */
        ui_putlim(x, y, buf + off, w, UI_ED_ATTR);
        ui_gotoxy(x + (cur - off), y);
        k = ui_getkey();
        if (k == KEY_ENTER) { ui_cursor(0); return 1; }
        if (k == KEY_ESC)   { ui_cursor(0); return 0; }
        else if (k == KEY_LEFT)  { if (cur > 0) cur--; }
        else if (k == KEY_RIGHT) { if (cur < len) cur++; }
        else if (k == KEY_HOME)  { cur = 0; }
        else if (k == KEY_END)   { cur = len; }
        else if (k == KEY_BKSP) {
            if (cur > 0) {
                memmove(&buf[cur - 1], &buf[cur], (size_t)(len - cur + 1));
                cur--; len--;
            }
        } else if (k == KEY_DEL) {
            if (cur < len) {
                memmove(&buf[cur], &buf[cur + 1], (size_t)(len - cur));
                len--;
            }
        } else if (k >= 32 && k <= 126) {
            if (len < maxlen - 1) {
                memmove(&buf[cur + 1], &buf[cur], (size_t)(len - cur + 1));
                buf[cur] = (char)k;
                cur++; len++;
            }
        }
    }
}
