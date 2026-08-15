/* ===================================================================
 * CFGEDIT.C  -  CASTALIA DOS Config Editor  (CFGEDIT.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * A small, self-contained line editor for CONFIG.SYS and AUTOEXEC.BAT.
 * It is line-oriented (not a full free-text editor): navigate lines,
 * edit the current line, insert or delete whole lines, and save.  On
 * save it FIRST copies the existing file to C:\CASTALIA\BACKUP, so an
 * edit can always be undone with SAFEBOOT.
 *
 * Being self-contained (no dependency on an external EDITOR) means it
 * still works from the rescue floppy.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os cfgedit.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/UI.H"

#define MAXL 300
#define MAXC 160

static char lines[MAXL][MAXC];
static int  nlines = 0;
static int  modified = 0;
static int  truncated = 0;

/* Editable targets. */
typedef struct { const char *path; const char *bak; const char *title; } TGT;
static TGT targets[] = {
    { "C:\\CONFIG.SYS",   "C:\\CASTALIA\\BACKUP\\CONFIG.SYS",   "CONFIG.SYS"   },
    { "C:\\AUTOEXEC.BAT", "C:\\CASTALIA\\BACKUP\\AUTOEXEC.BAT", "AUTOEXEC.BAT" }
};
#define NTGT 2

/* --- file helpers --------------------------------------------------- */

static int copyfile(const char *src, const char *dst)
{
    FILE *in, *out;
    char buf[2048];
    size_t n;
    in = fopen(src, "rb");
    if (in == NULL) return -1;
    out = fopen(dst, "wb");
    if (out == NULL) { fclose(in); return -2; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { fclose(in); fclose(out); return -3; }
    fclose(in);
    fclose(out);
    return 0;
}

static int file_exists(const char *p)
{
    FILE *fp = fopen(p, "rb");
    if (fp == NULL) return 0;
    fclose(fp);
    return 1;
}

static void strip_eol(char *s)
{
    int n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

static void load_file(const char *path)
{
    FILE *fp;
    char tmp[MAXC];

    nlines = 0;
    modified = 0;
    truncated = 0;
    fp = fopen(path, "r");
    if (fp == NULL) {
        /* New/empty file: start with one blank line. */
        lines[0][0] = '\0';
        nlines = 1;
        return;
    }
    while (fgets(tmp, (int)sizeof(tmp), fp) != NULL) {
        if (nlines >= MAXL) { truncated = 1; break; }
        strip_eol(tmp);
        strncpy(lines[nlines], tmp, MAXC - 1);
        lines[nlines][MAXC - 1] = '\0';
        nlines++;
    }
    fclose(fp);
    if (nlines == 0) { lines[0][0] = '\0'; nlines = 1; }
}

/* Save; back up the existing file first.  Returns 0 on success. */
static int save_file(const TGT *t)
{
    FILE *fp;
    int i;
    if (file_exists(t->path))
        copyfile(t->path, t->bak);      /* best effort backup */
    fp = fopen(t->path, "w");
    if (fp == NULL)
        return -1;
    for (i = 0; i < nlines; i++)
        fprintf(fp, "%s\r\n", lines[i]);
    fclose(fp);
    modified = 0;
    return 0;
}

/* Single-line editing comes from the shared UI library (ui_editline). */

/* --- editor screen -------------------------------------------------- */

#define LIST_Y 3
#define VIS   18
#define NUM_X 4
#define TXT_X 9
#define TXT_W 65

static void draw_editor(const TGT *t, int cur, int top)
{
    char line[80];
    int row, li;
    unsigned char attr;

    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA DOS", A_TITLE);
    sprintf(line, "Config Editor - %s%s", t->title,
            modified ? " [modified]" : "");
    ui_puts(SCR_W - (int)strlen(line) - 2, 0, line, UI_ATTR(C_WHITE, C_BLUE));

    ui_box(2, 2, 76, 20, A_FRAME);

    for (row = 0; row < VIS; row++) {
        li = top + row;
        ui_fill(3, LIST_Y + row, 74, 1, ' ', A_DESKTOP);
        if (li >= nlines)
            continue;
        attr = (li == cur) ? A_ITEMSEL : A_ITEM;
        if (li == cur)
            ui_fill(3, LIST_Y + row, 74, 1, ' ', attr);
        sprintf(line, "%4d", li + 1);
        ui_puts(NUM_X, LIST_Y + row, line,
                (li == cur) ? attr : A_HINT);
        ui_putlim(TXT_X, LIST_Y + row, lines[li], TXT_W, attr);
    }

    if (truncated)
        ui_puts(4, 22, "(file was longer than the editor limit; extra lines "
                "not loaded)", A_WARN);

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(1, SCR_H - 1,
        " \x18\x19 Move  Enter Edit  Ins Add  Del Remove  F2 Save  Esc Quit",
        A_STATUS);
}

/* Ask to save on quit.  Returns 1 to quit, 0 to stay. */
static int confirm_quit(const TGT *t)
{
    int w = 52, h = 8, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2, k;
    if (!modified)
        return 1;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " Unsaved changes ", A_PANELHDR);
    ui_puts(x + 3, y + 2, "Save changes before leaving?", A_PANEL);
    ui_puts(x + 3, y + 4, "S = save    D = discard    Esc = keep editing",
            A_PANEL);
    for (;;) {
        k = ui_getkey();
        if (k == 's' || k == 'S') { save_file(t); return 1; }
        if (k == 'd' || k == 'D') return 1;
        if (k == KEY_ESC)         return 0;
    }
}

static void notify(const char *msg)
{
    int w = 50, h = 5, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_putlim(x + 3, y + 2, msg, w - 6, A_PANEL);
    ui_getkey();
}

static void edit_target(const TGT *t)
{
    int cur = 0, top = 0, key, i;

    load_file(t->path);
    for (;;) {
        if (cur < top) top = cur;
        if (cur >= top + VIS) top = cur - VIS + 1;
        draw_editor(t, cur, top);
        key = ui_getkey();

        if (key == KEY_ESC) {
            if (confirm_quit(t)) return;
        } else if (key == KEY_UP) {
            if (cur > 0) cur--;
        } else if (key == KEY_DOWN) {
            if (cur < nlines - 1) cur++;
        } else if (key == KEY_PGUP) {
            cur -= VIS; if (cur < 0) cur = 0;
        } else if (key == KEY_PGDN) {
            cur += VIS; if (cur > nlines - 1) cur = nlines - 1;
        } else if (key == KEY_HOME) {
            cur = 0;
        } else if (key == KEY_END) {
            cur = nlines - 1;
        } else if (key == KEY_ENTER) {
            char work[MAXC];
            strcpy(work, lines[cur]);
            if (ui_editline(work, MAXC, TXT_X, LIST_Y + (cur - top), TXT_W)) {
                if (strcmp(work, lines[cur]) != 0) {
                    strcpy(lines[cur], work);
                    modified = 1;
                }
            }
        } else if (key == KEY_INS) {
            if (nlines < MAXL) {
                for (i = nlines; i > cur + 1; i--)
                    strcpy(lines[i], lines[i - 1]);
                lines[cur + 1][0] = '\0';
                nlines++;
                cur++;
                modified = 1;
            } else {
                notify("The editor is full; cannot add more lines.");
            }
        } else if (key == KEY_DEL) {
            if (nlines > 1) {
                for (i = cur; i < nlines - 1; i++)
                    strcpy(lines[i], lines[i + 1]);
                nlines--;
                if (cur >= nlines) cur = nlines - 1;
                modified = 1;
            } else {
                lines[0][0] = '\0';
                modified = 1;
            }
        } else if (key == KEY_F2) {
            if (save_file(t) == 0)
                notify("Saved.  Previous version backed up. Reboot to apply.");
            else
                notify("Save failed - is the disk writable?");
        }
    }
}

/* --- file chooser --------------------------------------------------- */

static int choose_file(void)
{
    int sel = 0, i, key;
    char line[60];
    unsigned char attr;

    for (;;) {
        ui_cls(A_DESKTOP);
        ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
        ui_puts(2, 0, "CASTALIA DOS", A_TITLE);
        ui_puts(SCR_W - 15, 0, "Config Editor", UI_ATTR(C_WHITE, C_BLUE));

        ui_center(6, "Which file do you want to edit?", A_TITLE);
        ui_box(28, 9, 24, NTGT + 2, A_FRAME);
        for (i = 0; i < NTGT; i++) {
            attr = (i == sel) ? A_ITEMSEL : A_ITEM;
            ui_fill(29, 10 + i, 22, 1, ' ', attr);
            sprintf(line, " %-20s", targets[i].title);
            ui_puts(29, 10 + i, line, attr);
        }
        ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
        ui_puts(2, SCR_H - 1, " Up/Down Select   Enter Edit   Esc Quit",
                A_STATUS);

        key = ui_getkey();
        if (key == KEY_ESC) return -1;
        else if (key == KEY_UP)   sel = (sel > 0) ? sel - 1 : NTGT - 1;
        else if (key == KEY_DOWN) sel = (sel < NTGT - 1) ? sel + 1 : 0;
        else if (key == KEY_ENTER) return sel;
    }
}

int main(void)
{
    int idx;
    ui_init();
    for (;;) {
        idx = choose_file();
        if (idx < 0)
            break;
        edit_target(&targets[idx]);
    }
    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
