/* ===================================================================
 * CASTEDIT.C  -  CASTALIA EDIT: general text editor  (CASTEDIT.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * The suite's general-purpose text editor: the proven line-oriented
 * engine from CFGEDIT, generalised to ANY file.  Open a file from the
 * command line (CASTEDIT NOTES.TXT) or from an in-program prompt,
 * navigate lines, edit the current line in place (shared ui_editline),
 * insert/delete lines, and save.
 *
 * Safety: on save, the previous version of the file is first copied to
 * a .BAK next to it (NOTES.TXT -> NOTES.BAK), so every save is
 * reversible.  F4 saves under a new name.  New files work: opening a
 * name that does not exist starts an empty buffer.
 *
 * Line-oriented on purpose (like CFGEDIT): tiny, predictable, and
 * plenty for CONFIG files, notes, INI files, and batch files.  Caps:
 * 400 lines x 160 characters, loaded fully into fixed buffers; longer
 * files load up to the cap with a visible warning.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os castedit.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/UI.H"

#define MAXL 400
#define MAXC 160

static char lines[MAXL][MAXC];
static int  nlines = 0;
static int  modified = 0;
static int  truncated = 0;
static char fname[80];
static int  have_name = 0;

/* --- file helpers ------------------------------------------------------ */

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

/* NOTES.TXT -> NOTES.BAK (extension replaced; appended when none). */
static void bak_name(const char *name, char *out)
{
    const char *dot = NULL;
    const char *p;
    int i;

    for (p = name; *p; p++) {
        if (*p == '\\' || *p == ':')
            dot = NULL;             /* dots before the basename don't count */
        else if (*p == '.')
            dot = p;
    }
    if (dot != NULL) {
        i = (int)(dot - name);
        memcpy(out, name, (size_t)i);
        strcpy(out + i, ".BAK");
    } else {
        sprintf(out, "%.70s.BAK", name);
    }
}

static void strip_eol(char *s)
{
    int n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

static void load_file(void)
{
    FILE *fp;
    char tmp[MAXC];

    nlines = 0;
    modified = 0;
    truncated = 0;
    fp = fopen(fname, "r");
    if (fp == NULL) {               /* new file: one blank line */
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

/* Save to fname; back up the old version first.  0 on success. */
static int save_file(void)
{
    FILE *fp;
    char bak[84];
    int i;

    if (!have_name)
        return -2;
    if (file_exists(fname)) {
        bak_name(fname, bak);
        copyfile(fname, bak);       /* best-effort backup */
    }
    fp = fopen(fname, "w");
    if (fp == NULL)
        return -1;
    for (i = 0; i < nlines; i++)
        fprintf(fp, "%s\r\n", lines[i]);
    fclose(fp);
    modified = 0;
    return 0;
}

/* --- dialogs ------------------------------------------------------------- */

static void notify(const char *msg)
{
    int w = 54, h = 5, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_putlim(x + 3, y + 2, msg, w - 6, A_PANEL);
    ui_getkey();
}

/* Prompt for a filename into buf.  1 = accepted non-empty. */
static int ask_name(const char *title, char *buf, int maxlen)
{
    int w = 60, h = 6, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, title, A_PANELHDR);
    if (!ui_editline(buf, maxlen, x + 3, y + 3, w - 6))
        return 0;
    return (buf[0] != '\0');
}

/* --- editor screen ----------------------------------------------------------- */

#define LIST_Y 3
#define VIS   18
#define NUM_X 4
#define TXT_X 9
#define TXT_W 65

static void draw_editor(int cur, int top)
{
    char line[80];
    int row, li;
    unsigned char attr;

    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA EDIT", A_TITLE);
    sprintf(line, "%.40s%s  (%d lines)",
            have_name ? fname : "(untitled)",
            modified ? " [modified]" : "", nlines);
    ui_puts(SCR_W - (int)strlen(line) - 2, 0, line,
            UI_ATTR(C_WHITE, C_BLUE));

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
        ui_puts(NUM_X, LIST_Y + row, line, (li == cur) ? attr : A_HINT);
        ui_putlim(TXT_X, LIST_Y + row, lines[li], TXT_W, attr);
    }

    if (truncated)
        ui_puts(4, 22, "(file was longer than the editor limit; extra lines "
                "not loaded)", A_WARN);

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(1, SCR_H - 1,
        " \x18\x19 Move  Enter Edit  Ins Add  Del Remove  F2 Save  "
        "F4 Save as  Esc Quit", A_STATUS);
}

/* Ask about unsaved changes on quit.  1 = leave, 0 = stay. */
static int confirm_quit(void)
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
        if (k == 's' || k == 'S') {
            if (!have_name && !ask_name(" Save as ", fname, sizeof(fname)))
                return 0;
            have_name = 1;
            if (save_file() != 0) {
                notify("Save failed - is the disk writable?");
                return 0;
            }
            return 1;
        }
        if (k == 'd' || k == 'D') return 1;
        if (k == KEY_ESC)         return 0;
    }
}

static void do_save(int saveas)
{
    if (saveas || !have_name) {
        char newname[80];
        strcpy(newname, have_name ? fname : "");
        if (!ask_name(" Save as ", newname, sizeof(newname)))
            return;
        strcpy(fname, newname);
        have_name = 1;
    }
    if (save_file() == 0)
        notify("Saved.  Previous version kept as .BAK.");
    else
        notify("Save failed - is the disk writable?");
}

/* --- main -------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    int cur = 0, top = 0, key, i;

    fname[0] = '\0';
    if (argc > 1) {
        strncpy(fname, argv[1], sizeof(fname) - 1);
        fname[sizeof(fname) - 1] = '\0';
        have_name = 1;
    }

    ui_init();

    if (!have_name) {
        ui_cls(A_DESKTOP);
        if (ask_name(" File to edit (new or existing) ",
                     fname, sizeof(fname)))
            have_name = 1;
        /* An empty answer starts an untitled buffer. */
    }
    load_file();

    for (;;) {
        if (cur < top) top = cur;
        if (cur >= top + VIS) top = cur - VIS + 1;
        draw_editor(cur, top);
        key = ui_getkey();

        if (key == KEY_ESC) {
            if (confirm_quit())
                break;
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
            do_save(0);
        } else if (key == KEY_F4) {
            do_save(1);
        }
    }

    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
