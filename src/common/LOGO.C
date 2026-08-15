/* ===================================================================
 * LOGO.C  -  Shared CASTALIA DOS text-mode logo art
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * See LOGO.H.  The keep is stored as printable ASCII and translated to
 * CP437 block glyphs at draw time, which keeps the source diff-friendly:
 *
 *   '#' steel wall   'M' amber merlon   'o' lit window   'G' dark gate
 *   ':' base shadow  '_' ground line    '|' flag pole    'P' amber flag
 *
 * The wordmark is a tiny 5x5 block font rendered on demand.
 *
 * Build (Open Watcom):  wcc -0 -bt=dos -ml -os logo.c
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <string.h>
#include "UI.H"
#include "LOGO.H"

/* --- The fortress keep ---------------------------------------------- */

static const char *keep_art[LOGO_KEEP_H] = {
    "       |P             |P             |P",
    "       |              |              |",
    "       |              |              |",
    "    M M M M      M M M M M M      M M M M",
    "    #######      ###########      #######",
    "    #o#o#o#M M M #o#o#o#o#o#M M M #o#o#o#",
    "    #####################################",
    "    ########o####o#o#ooo#o#o####o########",
    "    #################GGG#################",
    "    ################GGGGG################",
    "    ::::::::::::::::GGGGG::::::::::::::::",
    "  _________________________________________"
};

static void keep_cell(int x, int y, char c)
{
    char g;
    unsigned char a;

    switch (c) {
    case '#': g = (char)0xDB; a = UI_ATTR(C_LGRAY,  C_BLUE); break;
    case 'M': g = (char)0xDB; a = UI_ATTR(C_YELLOW, C_BLUE); break;
    case 'o': g = (char)0xFE; a = UI_ATTR(C_YELLOW, C_BLUE); break;
    case 'G': g = (char)0xDB; a = UI_ATTR(C_DGRAY,  C_BLUE); break;
    case ':': g = (char)0xB2; a = UI_ATTR(C_DGRAY,  C_BLUE); break;
    case '_': g = (char)0xDC; a = UI_ATTR(C_LGRAY,  C_BLUE); break;
    case '|': g = (char)0xB3; a = UI_ATTR(C_LGRAY,  C_BLUE); break;
    case 'P': g = (char)0xDB; a = UI_ATTR(C_YELLOW, C_BLUE); break;
    default:  g = ' ';        a = UI_ATTR(C_LGRAY,  C_BLUE); break;
    }
    ui_putc(x, y, g, a);
}

void logo_keep_row(int x, int y0, int r)
{
    const char *row;
    int i, len;

    if (r < 0 || r >= LOGO_KEEP_H)
        return;
    row = keep_art[r];
    len = (int)strlen(row);
    for (i = 0; i < len; i++)
        keep_cell(x + i, y0 + r, row[i]);
}

void logo_keep(int x, int y0)
{
    int r;
    for (r = 0; r < LOGO_KEEP_H; r++)
        logo_keep_row(x, y0, r);
}

/* Compact keep mark (menu / tool header crown). */
static const char *mark_art[LOGO_MARK_H] = {
    "    |P     |P     |P",
    "   M M   M M M   M M",
    "   ###   #####   ###",
    "   ###   ##o##   ###",
    "   ########G########",
    "   #######GGG#######",
    "  ___________________"
};

void logo_mark(int x, int y)
{
    int r, i, len;
    const char *row;
    for (r = 0; r < LOGO_MARK_H; r++) {
        row = mark_art[r];
        len = (int)strlen(row);
        for (i = 0; i < len; i++)
            keep_cell(x + i, y + r, row[i]);
    }
}

/* Repaint only the cells the art marks with 'c', in 'attr'.  The mark's
 * animation hooks are one-liners on top of this. */
static void mark_cells(int x, int y, char c, char glyph, unsigned char attr)
{
    int r, i, len;
    const char *row;
    for (r = 0; r < LOGO_MARK_H; r++) {
        row = mark_art[r];
        len = (int)strlen(row);
        for (i = 0; i < len; i++)
            if (row[i] == c)
                ui_putc(x + i, y + r, glyph, attr);
    }
}

void logo_mark_windows(int x, int y, unsigned char attr)
{
    mark_cells(x, y, 'o', (char)0xFE, attr);
}

void logo_mark_flags(int x, int y, unsigned char attr)
{
    mark_cells(x, y, 'P', (char)0xDB, attr);
}

void logo_keep_flags(int x, int y0, unsigned char attr)
{
    int r, i, len;
    const char *row;
    for (r = 0; r < 3 && r < LOGO_KEEP_H; r++) {
        row = keep_art[r];
        len = (int)strlen(row);
        for (i = 0; i < len; i++)
            if (row[i] == 'P')
                ui_putc(x + i, y0 + r, (char)0xDB, attr);
    }
}

void logo_keep_windows(int x, int y0, unsigned char attr)
{
    int r, i, len;
    const char *row;
    for (r = 0; r < LOGO_KEEP_H; r++) {
        row = keep_art[r];
        len = (int)strlen(row);
        for (i = 0; i < len; i++)
            if (row[i] == 'o')
                ui_putc(x + i, y0 + r, (char)0xFE, attr);
    }
}

/* --- The block-letter wordmark -------------------------------------- */

struct glyph {
    char        c;
    const char *row[LOGO_BIG_H];
};

/* 5-wide cells; '#' is a filled block, space is field.  Enough letters
 * and digits for CASTALIA DOS, the edition string, and the installer's
 * WELCOME / INSTALLED / RESCUE / READY headings. */
static const struct glyph font[] = {
    {'A', {" ### ", "#   #", "#####", "#   #", "#   #"}},
    {'B', {"#### ", "#   #", "#### ", "#   #", "#### "}},
    {'C', {"#####", "#    ", "#    ", "#    ", "#####"}},
    {'D', {"#### ", "#   #", "#   #", "#   #", "#### "}},
    {'E', {"#####", "#    ", "#### ", "#    ", "#####"}},
    {'G', {"#####", "#    ", "#  ##", "#   #", "#####"}},
    {'I', {"#####", "  #  ", "  #  ", "  #  ", "#####"}},
    {'L', {"#    ", "#    ", "#    ", "#    ", "#####"}},
    {'M', {"#   #", "## ##", "# # #", "#   #", "#   #"}},
    {'N', {"#   #", "##  #", "# # #", "#  ##", "#   #"}},
    {'O', {" ### ", "#   #", "#   #", "#   #", " ### "}},
    {'R', {"#### ", "#   #", "#### ", "#  # ", "#   #"}},
    {'S', {"#####", "#    ", "#####", "    #", "#####"}},
    {'T', {"#####", "  #  ", "  #  ", "  #  ", "  #  "}},
    {'U', {"#   #", "#   #", "#   #", "#   #", "#####"}},
    {'V', {"#   #", "#   #", "#   #", " # # ", "  #  "}},
    {'W', {"#   #", "#   #", "# # #", "## ##", "#   #"}},
    {'Y', {"#   #", " # # ", "  #  ", "  #  ", "  #  "}},
    {'0', {" ### ", "#  ##", "# # #", "##  #", " ### "}},
    {'1', {"  #  ", " ##  ", "  #  ", "  #  ", "#####"}},
    {'3', {"#### ", "    #", " ### ", "    #", "#### "}},
    {'6', {" ####", "#    ", "#### ", "#   #", " ### "}},
    {'8', {" ### ", "#   #", " ### ", "#   #", " ### "}},
    {'X', {"#   #", " # # ", "  #  ", " # # ", "#   #"}}
};
#define NGLYPH ((int)(sizeof(font) / sizeof(font[0])))

static const struct glyph *find_glyph(char c)
{
    int i;
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');
    for (i = 0; i < NGLYPH; i++)
        if (font[i].c == c)
            return &font[i];
    return 0;
}

int logo_bigtext(int x, int y, const char *s, unsigned char attr)
{
    int cx = x;
    const char *p;

    for (p = s; *p != '\0'; p++) {
        const struct glyph *gph = find_glyph(*p);
        int r;
        for (r = 0; r < LOGO_BIG_H; r++) {
            const char *cells = gph ? gph->row[r] : "     ";
            int i;
            for (i = 0; i < 5; i++)
                ui_putc(cx + i, y + r,
                        (cells[i] == '#') ? (char)0xDB : ' ', attr);
        }
        cx += 6;                        /* 5-wide glyph + 1 column gap */
    }
    return cx - x;
}

int logo_bigtext_width(const char *s)
{
    int n = (int)strlen(s);
    return (n > 0) ? (n * 6 - 1) : 0;   /* drop the final trailing gap */
}

void logo_bigtext_center(int y, const char *s, unsigned char attr)
{
    int w = logo_bigtext_width(s);
    int x = (SCR_W - w) / 2;
    if (x < 0)
        x = 0;
    logo_bigtext(x, y, s, attr);
}
