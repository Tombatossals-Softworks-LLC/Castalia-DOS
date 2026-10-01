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
 * Safety: a save writes the whole buffer to a temporary file next to
 * the target and only swaps it in once every byte is on the disk, so a
 * full disk can never leave a truncated file.  On the first save of a
 * session the version that was opened is copied to a .BAK next to it
 * (NOTES.TXT -> NOTES.BAK); later saves keep that .BAK, so it always
 * holds the file as it was before this session.  F4 saves under a new
 * name.  New files work: opening a name that does not exist starts an
 * empty buffer.
 *
 * Line-oriented on purpose (like CFGEDIT): tiny, predictable, and
 * plenty for CONFIG files, notes, INI files, and batch files.  Caps:
 * 400 lines x 159 characters, loaded fully into fixed buffers.  A file
 * beyond either cap is shown up to the cap and opened read-only:
 * saving it would silently cut off what did not fit.
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
#include "../common/SAFEIO.H"

#define MAXL 400
#define MAXC 160

static char lines[MAXL][MAXC];
static int  nlines = 0;
static int  modified = 0;
static int  truncated = 0;      /* file did not fit: read-only */
static char fname[80];
static int  have_name = 0;
static char bak_of[80];         /* file whose .BAK this session made */
static int  bak_made = 0;       /* ... and whether there was one     */

/* --- file helpers ------------------------------------------------------ */

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
    char tmp[MAXC + 2];             /* a full line plus its '\n' */
    int n;

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

/* Save to fname.  0 on success; -1 write failed, -2 no name,
 * -3 read-only, -4 the .BAK could not be made.  On any failure the file
 * on disk is as it was. */
static int save_file(void)
{
    FILE *fp;
    char tmp[SIO_PATH], bak[84];
    int i, bad = 0;

    if (!have_name)
        return -2;
    if (truncated)
        return -3;
    if (sio_tmpname(fname, tmp) != SIO_OK)
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

    /* Back up only on the first save of this file in this session:
     * a second save must not replace the .BAK of the original with
     * the first save's version. */
    if (bak_of[0] == '\0' || !sio_same(bak_of, fname)) {
        bak_made = 0;
        if (sio_exists(fname)) {
            bak_name(fname, bak);
            if (!sio_same(fname, bak)) {    /* editing NOTES.BAK itself */
                if (sio_copy(fname, bak) != SIO_OK) {
                    remove(tmp);
                    return -4;
                }
                bak_made = 1;
            }
        }
        strcpy(bak_of, fname);
    }

    if (sio_replace(tmp, fname) != SIO_OK)
        return -1;
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

/* Tell the user why a save did not happen. */
static void save_failed(int rc)
{
    char msg[60];
    int crit = ui_crit_take();

    if (rc == -3)
        strcpy(msg, "Read-only file (too big to load); not saved.");
    else if (crit >= 0)
        sprintf(msg, "Save failed: %s.", ui_crit_text(crit));
    else if (rc == -4)
        strcpy(msg, "Backup (.BAK) failed; the file is unchanged.");
    else
        strcpy(msg, "Save failed; the file on disk is unchanged.");
    notify(msg);
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
        ui_puts(4, 22, "Read-only: beyond 400 lines or 159 characters a line; "
                "not all shown", A_WARN);

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(1, SCR_H - 1,
        " \x18\x19 Move  Enter Edit  Ins Add  Del Remove  F2 Save  "
        "F4 Save as  Esc Quit", A_STATUS);
}

/* Ask about unsaved changes on quit.  1 = leave, 0 = stay. */
static int confirm_quit(void)
{
    int w = 52, h = 8, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2, k, i;
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
            i = save_file();
            if (i != 0) {
                save_failed(i);
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
    int rc;

    if (truncated) {                /* before asking for a name */
        save_failed(-3);
        return;
    }
    if (saveas || !have_name) {
        char newname[80];
        strcpy(newname, have_name ? fname : "");
        if (!ask_name(" Save as ", newname, sizeof(newname)))
            return;
        strcpy(fname, newname);
        have_name = 1;
    }
    rc = save_file();
    if (rc != 0)
        save_failed(rc);
    else if (bak_made)
        notify("Saved.  The version you opened is kept as .BAK.");
    else
        notify("Saved.");
}

/* Editing keys on a read-only buffer: say why nothing happens.
 * 1 if the key must be ignored. */
static int read_only(void)
{
    if (!truncated)
        return 0;
    notify("Read-only: the file is too big for the editor.");
    return 1;
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
        } else if ((key == KEY_ENTER || key == KEY_INS || key == KEY_DEL)
                   && read_only()) {
            /* nothing: the buffer does not hold the whole file */
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
