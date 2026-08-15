/* ===================================================================
 * HELP.C  -  CASTALIA DOS Help Reader  (HELP.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * The on-disk help system's reader (designed in docs/HELP.md): shows
 * the topic index from HELP.IDX and pages through the plain-text help
 * pages in C:\CASTALIA\HELP.  The pages are pre-wrapped 80-column text,
 * so no formatting engine is needed - fast on a 386SX and trivial to
 * author or translate.
 *
 *   HELP            open the topic index
 *   HELP SOUND      jump straight to a topic (id or unique prefix)
 *
 * HELP.IDX is plain INI ([id] title=... file=...), read with the shared
 * INI module, so new pages can be added without recompiling.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os help.c ..\common\ini.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/INI.H"
#include "../common/UI.H"

#define MAXTOP 24
#define MAXVL  400          /* viewer line cap  */
#define MAXVC  96           /* viewer line width */

typedef struct {
    char id[16];
    char title[40];
    char file[16];
} TOPIC;

static TOPIC topics[MAXTOP];
static int   ntop = 0;
static char  helpdir[64];   /* directory of HELP.IDX, with trailing '\' */
static char  note[64];      /* one-line notice on the index screen      */

static char  vlines[MAXVL][MAXVC];
static int   vn = 0;

/* --- small helpers ---------------------------------------------------- */

static void copystr(char *dst, const char *src, int size)
{
    if (src == NULL) { dst[0] = '\0'; return; }
    strncpy(dst, src, (size_t)(size - 1));
    dst[size - 1] = '\0';
}

static char up(char c)
{
    if (c >= 'a' && c <= 'z') return (char)(c - 32);
    return c;
}

static int ci_prefix(const char *pat, const char *s)
{
    int i;
    for (i = 0; pat[i]; i++)
        if (up(pat[i]) != up(s[i]))
            return 0;
    return 1;                       /* pat is a prefix of s */
}

/* --- index loading ------------------------------------------------------ */

static int load_index(void)
{
    static const struct { const char *idx; const char *dir; } cand[] = {
        { "C:\\CASTALIA\\HELP\\HELP.IDX", "C:\\CASTALIA\\HELP\\" },
        { "HELP.IDX",                     ""                     },
        { "help\\HELP.IDX",               "help\\"               }
    };
    int i, j, n;
    const char *sec, *title, *file;

    for (i = 0; i < (int)(sizeof(cand) / sizeof(cand[0])); i++) {
        if (ini_open(cand[i].idx) == INI_OK) {
            strcpy(helpdir, cand[i].dir);
            n = ini_section_count();
            for (j = 0; j < n && ntop < MAXTOP; j++) {
                sec = ini_section_name(j);
                if (sec == NULL)
                    continue;
                title = ini_get(sec, "title");
                file  = ini_get(sec, "file");
                if (title == NULL || file == NULL)
                    continue;
                copystr(topics[ntop].id, sec, sizeof(topics[0].id));
                copystr(topics[ntop].title, title, sizeof(topics[0].title));
                copystr(topics[ntop].file, file, sizeof(topics[0].file));
                ntop++;
            }
            return (ntop > 0);
        }
    }
    return 0;
}

/* Find a topic by id (exact match first, then unique-enough prefix). */
static int find_topic(const char *pat)
{
    int i;
    for (i = 0; i < ntop; i++) {
        int j;
        for (j = 0; topics[i].id[j] && pat[j]; j++)
            if (up(topics[i].id[j]) != up(pat[j]))
                break;
        if (topics[i].id[j] == '\0' && pat[j] == '\0')
            return i;               /* exact */
    }
    for (i = 0; i < ntop; i++)
        if (ci_prefix(pat, topics[i].id))
            return i;               /* first prefix match */
    return -1;
}

/* --- the reader ----------------------------------------------------------- */

static int load_page(const char *file)
{
    char path[96], tmp[MAXVC];
    FILE *fp;

    vn = 0;
    sprintf(path, "%s%s", helpdir, file);
    fp = fopen(path, "r");
    if (fp == NULL)
        return 0;
    while (fgets(tmp, (int)sizeof(tmp), fp) != NULL && vn < MAXVL) {
        int n = (int)strlen(tmp);
        while (n > 0 && (tmp[n - 1] == '\n' || tmp[n - 1] == '\r'))
            tmp[--n] = '\0';
        strncpy(vlines[vn], tmp, MAXVC - 1);
        vlines[vn][MAXVC - 1] = '\0';
        vn++;
    }
    fclose(fp);
    return 1;
}

static void read_topic(int t)
{
    int top = 0, key, row, li;
    int vis = SCR_H - 4;
    char buf[64];

    if (!load_page(topics[t].file)) {
        note[0] = '\0';
        sprintf(note, "Page missing: %s", topics[t].file);
        return;
    }

    for (;;) {
        ui_cls(A_DESKTOP);
        ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
        ui_puts(2, 0, "CASTALIA DOS Help", A_TITLE);
        ui_putlim(SCR_W - 40, 0, topics[t].title, 30,
                  UI_ATTR(C_WHITE, C_BLUE));
        sprintf(buf, "%d/%d", (vn > 0) ? (top + 1) : 0, vn);
        ui_puts(SCR_W - 8, 0, buf, A_HINT);

        ui_hline(0, 1, SCR_W, A_FRAME);
        for (row = 0; row < vis; row++) {
            li = top + row;
            if (li >= vn)
                break;
            ui_putlim(1, 2 + row, vlines[li], SCR_W - 2, A_ITEM);
        }

        ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
        ui_puts(2, SCR_H - 1,
                " PgUp/PgDn Page   Up/Down Scroll   Esc Topics", A_STATUS);

        key = ui_getkey();
        if (key == KEY_ESC)
            return;
        else if (key == KEY_DOWN) { if (top < vn - 1) top++; }
        else if (key == KEY_UP)   { if (top > 0) top--; }
        else if (key == KEY_PGDN) {
            top += vis;
            if (top > vn - 1) top = vn - 1;
            if (top < 0) top = 0;
        }
        else if (key == KEY_PGUP) { top -= vis; if (top < 0) top = 0; }
        else if (key == KEY_HOME) { top = 0; }
        else if (key == KEY_END)  { top = (vn > vis) ? vn - vis : 0; }
    }
}

/* --- the index ---------------------------------------------------------------- */

static void run_index(void)
{
    int sel = 0, top = 0, key, i, row;
    int vis = 16;
    char buf[60];

    for (;;) {
        if (sel < top) top = sel;
        if (sel >= top + vis) top = sel - vis + 1;

        ui_cls(A_DESKTOP);
        ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
        ui_puts(2, 0, "CASTALIA DOS Help", A_TITLE);
        ui_puts(SCR_W - 15, 0, "Topic Index", UI_ATTR(C_WHITE, C_BLUE));

        ui_box(14, 2, 52, vis + 2, A_FRAME);
        ui_puts(16, 2, " Topics ", A_TITLE);
        for (row = 0; row < vis; row++) {
            i = top + row;
            ui_fill(15, 3 + row, 50, 1, ' ', A_DESKTOP);
            if (i >= ntop)
                continue;
            {
                unsigned char a = (i == sel) ? A_ITEMSEL : A_ITEM;
                if (i == sel)
                    ui_fill(15, 3 + row, 50, 1, ' ', a);
                sprintf(buf, " %-10.10s %-34.34s",
                        topics[i].id, topics[i].title);
                ui_puts(16, 3 + row, buf, a);
            }
        }

        if (note[0])
            ui_center(21, note, A_WARN);
        ui_center(22,
            "Tip: jump straight to a topic with  HELP <name>",
            A_HINT);

        ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
        ui_puts(2, SCR_H - 1,
                " Up/Down Select   Enter Read   Esc Exit", A_STATUS);

        key = ui_getkey();
        if (key == KEY_ESC)
            return;
        else if (key == KEY_UP)
            sel = (sel > 0) ? sel - 1 : ntop - 1;
        else if (key == KEY_DOWN)
            sel = (sel < ntop - 1) ? sel + 1 : 0;
        else if (key == KEY_HOME)
            sel = 0;
        else if (key == KEY_END)
            sel = ntop - 1;
        else if (key == KEY_ENTER) {
            note[0] = '\0';
            read_topic(sel);
        }
    }
}

/* --- main ----------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    int t;

    note[0] = '\0';
    if (!load_index()) {
        printf("CASTALIA HELP: HELP.IDX not found.\n");
        printf("Expected at C:\\CASTALIA\\HELP\\HELP.IDX\n");
        return 1;
    }

    ui_init();

    if (argc > 1) {
        t = find_topic(argv[1]);
        if (t >= 0)
            read_topic(t);
        else
            sprintf(note, "No topic matches \"%.20s\".", argv[1]);
    }
    run_index();

    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
