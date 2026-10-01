/* ===================================================================
 * CFGEDIT.C  -  CASTALIA DOS Config Editor  (CFGEDIT.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * A small, self-contained line editor for CONFIG.SYS and AUTOEXEC.BAT.
 * It is line-oriented (not a full free-text editor): navigate lines,
 * edit the current line, insert or delete whole lines, and save.  The
 * first save of each file in a run FIRST copies the file as it was to
 * C:\CASTALIA\BACKUP, so an edit can always be undone with SAFEBOOT;
 * later saves in the same run keep that copy rather than replacing it
 * with an intermediate version.  The new file is written under a
 * temporary name and swapped in only once it is complete, so a full
 * disk can never leave a truncated CONFIG.SYS behind.
 *
 * A file beyond the editor's caps (300 lines, 159 characters a line) is
 * shown up to the cap and opened read-only: saving it would silently
 * drop what did not fit.
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
#include "../common/DIRW.H"
#include "../common/SAFEIO.H"

#define MAXL 300
#define MAXC 160

static char lines[MAXL][MAXC];
static int  nlines = 0;
static int  modified = 0;
static int  truncated = 0;      /* file did not fit: read-only */

/* Editable targets. */
typedef struct { const char *path; const char *bak; const char *title; } TGT;
static TGT targets[] = {
    { "C:\\CONFIG.SYS",   "C:\\CASTALIA\\BACKUP\\CONFIG.SYS",   "CONFIG.SYS"   },
    { "C:\\AUTOEXEC.BAT", "C:\\CASTALIA\\BACKUP\\AUTOEXEC.BAT", "AUTOEXEC.BAT" }
};
#define NTGT 2

/* Set once a target has been backed up in this run. */
static int backed_up[NTGT];

/* --- file helpers --------------------------------------------------- */

static void strip_eol(char *s)
{
    int n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

static void load_file(const char *path)
{
    FILE *fp;
    char tmp[MAXC + 2];             /* a full line plus its '\n' */
    int n;

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
        n = (int)strlen(tmp);
        if (n > 0 && tmp[n - 1] != '\n' && !feof(fp)) {
            /* fgets stopped mid-line.  Skip the rest of it rather than
             * let it come back as a line of its own. */
            int c;
            while ((c = fgetc(fp)) != EOF && c != '\n')
                ;
            truncated = 1;
        }
        strip_eol(tmp);
        if ((int)strlen(tmp) > MAXC - 1)
            truncated = 1;
        strncpy(lines[nlines], tmp, MAXC - 1);
        lines[nlines][MAXC - 1] = '\0';
        nlines++;
    }
    fclose(fp);
    if (nlines == 0) { lines[0][0] = '\0'; nlines = 1; }
}

/* Save.  Returns 0 on success; -1 write failed, -3 read-only, -4 the
 * backup could not be made.  On any failure the file on disk is as it
 * was. */
static int save_file(const TGT *t)
{
    FILE *fp;
    char tmp[SIO_PATH];
    int i, bad = 0, idx = (int)(t - targets);

    if (truncated)
        return -3;
    if (sio_tmpname(t->path, tmp) != SIO_OK)
        return -1;

    /* Binary mode: the lines carry their own CR LF, and text mode
     * would turn each "\r\n" into CR CR LF. */
    fp = fopen(tmp, "wb");
    if (fp == NULL)
        return -1;
    for (i = 0; i < nlines && !bad; i++)
        if (fputs(lines[i], fp) < 0 || fputs("\r\n", fp) < 0)
            bad = 1;
    if (sio_close(fp) != 0 || bad) {
        remove(tmp);
        return -1;
    }

    /* The backup is what SAFEBOOT restores, so a boot file is never
     * replaced without one; one per run, holding the file as it was
     * before this run's first save. */
    if (!backed_up[idx] && sio_exists(t->path)) {
        dirw_mkdir("C:\\CASTALIA\\BACKUP");     /* fails if it exists */
        if (sio_copy(t->path, t->bak) != SIO_OK) {
            remove(tmp);
            return -4;
        }
    }
    backed_up[idx] = 1;

    if (sio_replace(tmp, t->path) != SIO_OK)
        return -1;
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
        ui_puts(4, 22, "Read-only: beyond 300 lines or 159 characters a line; "
                "not all shown", A_WARN);

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(1, SCR_H - 1,
        " \x18\x19 Move  Enter Edit  Ins Add  Del Remove  F2 Save  Esc Quit",
        A_STATUS);
}

static void notify(const char *msg)
{
    int w = 50, h = 5, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_putlim(x + 3, y + 2, msg, w - 6, A_PANEL);
    ui_getkey();
}

/* Tell the user why a save did not happen. */
static void save_failed(int rc)
{
    char msg[48];
    int crit = ui_crit_take();

    if (rc == -3)
        strcpy(msg, "Read-only (file too big to load); not saved.");
    else if (crit >= 0)
        sprintf(msg, "Save failed: %s.", ui_crit_text(crit));
    else if (rc == -4)
        strcpy(msg, "Backup failed; the file is unchanged.");
    else
        strcpy(msg, "Save failed; the file on disk is unchanged.");
    notify(msg);
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
        if (k == 's' || k == 'S') {
            /* Leaving after a failed save would lose the edits while
             * the user believes they are on the disk. */
            k = save_file(t);
            if (k != 0) {
                save_failed(k);
                return 0;
            }
            return 1;
        }
        if (k == 'd' || k == 'D') return 1;
        if (k == KEY_ESC)         return 0;
    }
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
        } else if ((key == KEY_ENTER || key == KEY_INS || key == KEY_DEL)
                   && truncated) {
            notify("Read-only: the file is too big for the editor.");
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
            i = save_file(t);
            if (i == 0)
                notify("Saved.  Original backed up. Reboot to apply.");
            else
                save_failed(i);
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
