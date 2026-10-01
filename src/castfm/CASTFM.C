/* ===================================================================
 * CASTFM.C  -  CASTALIA DOS File Manager  (CASTFM.EXE / CFM.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * A light, single-pane, keyboard-first file manager in the spirit of
 * Norton Commander but far simpler.  It lists a directory, enters
 * subdirectories, changes drives, shows free space, views text files,
 * runs programs, and does copy / move / delete / make-directory.  This
 * is the "1.1" tool from docs/FILE-MANAGER.md, kept minimal.
 *
 * Portable directory search: Turbo C uses findfirst/ffblk, Open Watcom
 * uses _dos_findfirst/find_t; both paths are provided.  Drive and free-
 * space queries use INT 21h (portable).  No dynamic allocation.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os castfm.c ..\common\ui.c
 *
 * C89 only.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#if defined(__WATCOMC__)
#include <direct.h>
#elif defined(__TURBOC__)
#include <dir.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif
#include "../common/UI.H"
#include "../common/SAFEIO.H"

#define MAX_ENT  512
#define NAMELEN  13
#define MAXVL    400        /* text-viewer line cap  */
#define MAXVC    96         /* text-viewer line width*/

typedef struct {
    char name[NAMELEN];
    unsigned long size;
    int is_dir;
} FENT;

static FENT ents[MAX_ENT];
static int  nent = 0;
static char curdir[128];

/* --- portable directory search: shared module ----------------------- */
#include "../common/DIRW.H"
#define f_first  dirw_first
#define f_next   dirw_next
#define f_name   dirw_name
#define f_size   dirw_size
#define f_isdir  dirw_isdir

#define make_dir dirw_mkdir     /* shared portable wrapper */

/* --- INT 21h helpers (portable) ------------------------------------- */

static int cur_drive(void)
{
    union REGS r;
    r.h.ah = 0x19;
    int86(0x21, &r, &r);
    return r.h.al + 'A';
}

static void set_drive(int letter)
{
    union REGS r;
    r.h.ah = 0x0E;
    r.h.dl = (unsigned char)(letter - 'A');
    int86(0x21, &r, &r);
}

static unsigned long disk_free(void)
{
    union REGS r;
    r.h.ah = 0x36;
    r.h.dl = 0x00;
    int86(0x21, &r, &r);
    if (r.x.ax == 0xFFFF)
        return 0UL;
    return (unsigned long)r.x.ax *
           (unsigned long)r.x.bx *
           (unsigned long)r.x.cx;
}

/* --- listing -------------------------------------------------------- */

static int name_cmp(const void *a, const void *b)
{
    const FENT *x = (const FENT *)a;
    const FENT *y = (const FENT *)b;
    if (x->is_dir != y->is_dir)
        return y->is_dir - x->is_dir;   /* directories first */
    return strcmp(x->name, y->name);
}

static void read_dir(void)
{
    nent = 0;
    if (getcwd(curdir, sizeof(curdir)) == NULL)
        strcpy(curdir, "?");
    if (f_first("*.*") == 0) {
        do {
            const char *nm = f_name();
            if (strcmp(nm, ".") == 0)
                continue;                /* skip self */
            if (nent < MAX_ENT) {
                strncpy(ents[nent].name, nm, NAMELEN - 1);
                ents[nent].name[NAMELEN - 1] = '\0';
                ents[nent].size = f_size();
                ents[nent].is_dir = f_isdir();
                nent++;
            }
        } while (f_next() == 0);
    }
    qsort(ents, (size_t)nent, sizeof(FENT), name_cmp);
}

/* --- small utilities ------------------------------------------------ */

static char up(char c)
{
    if (c >= 'a' && c <= 'z') return (char)(c - 32);
    return c;
}

/* Turn the typed copy/move target into a file name.  The prompt says
 * "path or name", so a target that is a directory ("A:\", "..",
 * "C:\GAMES") means "into that directory, under the same name".
 * 0, or -1 if the result would not fit or the target has wildcards. */
static int resolve_dest(const char *typed, const char *name, char *out)
{
    int n = (int)strlen(typed);
    int dir;

    if (strchr(typed, '*') != NULL || strchr(typed, '?') != NULL)
        return -1;
    if (n + 1 + (int)strlen(name) + 1 > SIO_PATH)
        return -1;
    strcpy(out, typed);
    if (n > 0 && (typed[n - 1] == '\\' || typed[n - 1] == ':'))
        dir = 1;
    else
        dir = (f_first(typed) == 0 && f_isdir());
    if (dir) {
        if (typed[n - 1] != '\\' && typed[n - 1] != ':')
            strcat(out, "\\");
        strcat(out, name);
    }
    return 0;
}

/* Why the last copy failed, for the second line of a notice. */
static const char *copy_why(int rc)
{
    int crit = ui_crit_take();
    if (crit >= 0)                  /* the drive itself said why */
        return ui_crit_text(crit);
    if (rc == SIO_ERR_SRC)  return "cannot read the source file";
    if (rc == SIO_ERR_SAME) return "source and destination are one file";
    if (rc == SIO_ERR_NAME) return "the path is too long";
    return "cannot write there (disk full or write-protected?)";
}

static int is_exec(const char *name)
{
    const char *dot = strrchr(name, '.');
    char e[4];
    int i;
    if (dot == NULL) return 0;
    for (i = 0; i < 3 && dot[i + 1]; i++) e[i] = up(dot[i + 1]);
    e[i] = '\0';
    return (strcmp(e, "EXE") == 0 || strcmp(e, "COM") == 0 ||
            strcmp(e, "BAT") == 0);
}

/* --- inline single-line editor (for prompts) ------------------------ */

#define ED_ATTR UI_ATTR(C_WHITE, C_BLACK)

static int edit_line(char *buf, int maxlen, int x, int y, int w)
{
    int len = (int)strlen(buf), cur = (int)strlen(buf), off = 0, k, i;
    ui_cursor(1);
    for (;;) {
        if (cur < off) off = cur;
        if (cur >= off + w) off = cur - w + 1;
        ui_fill(x, y, w, 1, ' ', ED_ATTR);
        for (i = 0; i < w && (off + i) < len; i++)
            ui_putc(x + i, y, buf[off + i], ED_ATTR);
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
                buf[cur] = (char)k; cur++; len++;
            }
        }
    }
}

/* Prompt for a string.  Returns 1 if accepted. */
static int prompt(const char *title, char *buf, int maxlen)
{
    int w = 60, h = 6, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2, r;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, title, A_PANELHDR);
    r = edit_line(buf, maxlen, x + 3, y + 3, w - 6);
    return r;
}

static int confirm(const char *l1, const char *l2)
{
    int w = 56, h = 7, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2, k;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    if (l1) ui_putlim(x + 3, y + 2, l1, w - 6, A_PANEL);
    if (l2) ui_putlim(x + 3, y + 3, l2, w - 6, A_PANEL);
    ui_puts(x + 3, y + 5, "Enter/Y = yes    Esc/N = no", A_PANEL);
    for (;;) {
        k = ui_getkey();
        if (k == KEY_ENTER || k == 'y' || k == 'Y') return 1;
        if (k == KEY_ESC   || k == 'n' || k == 'N') return 0;
    }
}

static void notify(const char *l1, const char *l2)
{
    int w = 56, h = 6, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    if (l1) ui_putlim(x + 3, y + 2, l1, w - 6, A_PANEL);
    if (l2) ui_putlim(x + 3, y + 3, l2, w - 6, A_PANEL);
    ui_getkey();
}

/* --- text viewer ---------------------------------------------------- */

static char vlines[MAXVL][MAXVC];
static int  vn = 0;

static void view_file(const char *name)
{
    FILE *fp;
    char tmp[MAXVC];
    int top = 0, key, row, li;
    int vis = SCR_H - 3;

    vn = 0;
    fp = fopen(name, "r");
    if (fp == NULL) {
        notify("Cannot open the file.", name);
        return;
    }
    while (fgets(tmp, (int)sizeof(tmp), fp) != NULL && vn < MAXVL) {
        int n = (int)strlen(tmp);
        while (n > 0 && (tmp[n - 1] == '\n' || tmp[n - 1] == '\r'))
            tmp[--n] = '\0';
        strncpy(vlines[vn], tmp, MAXVC - 1);
        vlines[vn][MAXVC - 1] = '\0';
        vn++;
    }
    fclose(fp);

    for (;;) {
        ui_cls(A_DESKTOP);
        ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
        ui_puts(2, 0, "View", A_TITLE);
        ui_putlim(10, 0, name, 40, UI_ATTR(C_WHITE, C_BLUE));
        for (row = 0; row < vis; row++) {
            li = top + row;
            if (li >= vn) break;
            ui_putlim(1, 1 + row, vlines[li], SCR_W - 2, A_ITEM);
        }
        ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
        ui_puts(2, SCR_H - 1,
            " PgUp/PgDn  Up/Down  Esc Close", A_STATUS);
        key = ui_getkey();
        if (key == KEY_ESC) break;
        else if (key == KEY_DOWN) { if (top < vn - 1) top++; }
        else if (key == KEY_UP)   { if (top > 0) top--; }
        else if (key == KEY_PGDN) { top += vis; if (top > vn - 1) top = vn - 1; if (top < 0) top = 0; }
        else if (key == KEY_PGUP) { top -= vis; if (top < 0) top = 0; }
        else if (key == KEY_HOME) { top = 0; }
        else if (key == KEY_END)  { top = (vn > vis) ? vn - vis : 0; }
    }
}

/* --- main screen ---------------------------------------------------- */

#define LIST_Y 3
#define VIS   18

static void draw(int sel, int top)
{
    char line[80], sz[16];
    int row, gi;
    unsigned char attr;
    unsigned long fk;

    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA DOS", A_TITLE);
    ui_puts(SCR_W - 14, 0, "File Manager", UI_ATTR(C_WHITE, C_BLUE));

    fk = disk_free() / 1024UL;
    sprintf(line, "%-50.50s   %lu KB free", curdir, fk);
    ui_puts(1, 1, line, UI_ATTR(C_YELLOW, C_BLUE));

    ui_box(0, 2, SCR_W, 20, A_FRAME);
    for (row = 0; row < VIS; row++) {
        gi = top + row;
        ui_fill(1, LIST_Y + row, SCR_W - 2, 1, ' ', A_DESKTOP);
        if (gi >= nent) continue;
        attr = (gi == sel) ? A_ITEMSEL : A_ITEM;
        if (gi == sel) ui_fill(1, LIST_Y + row, SCR_W - 2, 1, ' ', attr);
        if (ents[gi].is_dir)
            strcpy(sz, "<DIR>");
        else
            sprintf(sz, "%lu", ents[gi].size);
        sprintf(line, " %-13.13s %12s", ents[gi].name, sz);
        ui_puts(2, LIST_Y + row, line, attr);
    }

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(0, SCR_H - 1,
        " Enter Open  F3 View  F4 Run  F5 Copy  F6 Move  F7 MkDir  "
        "F8 Del  F9 Drive  F10 Quit", A_STATUS);
}

/* --- copy / move ---------------------------------------------------- */

/* F5: copy the file 'name' (in the current directory) to 'typed'. */
static void copy_file(const char *name, const char *typed)
{
    char dst[SIO_PATH];
    int rc;

    if (resolve_dest(typed, name, dst) != 0) {
        notify("Copy failed: not a usable destination.", typed);
        return;
    }
    /* Opening the destination would empty the source before a byte of
     * it was read, and DOS spells one file many ways (X, .\X, C:x). */
    if (sio_same(name, dst)) {
        notify("Source and destination are the same file.", dst);
        return;
    }
    if (sio_exists(dst) && !confirm("Overwrite the existing file?", dst))
        return;
    ui_crit_take();                 /* report only this copy's error */
    rc = sio_copy(name, dst);
    if (rc != SIO_OK)
        notify("Copy failed; the destination was not changed.",
               copy_why(rc));
}

/* F6: move the file 'name' to 'typed'. */
static void move_file(const char *name, const char *typed)
{
    char dst[SIO_PATH];
    int rc, existed;

    if (resolve_dest(typed, name, dst) != 0) {
        notify("Move failed: not a usable destination.", typed);
        return;
    }
    if (sio_same(name, dst)) {
        notify("Source and destination are the same file.", dst);
        return;
    }
    existed = sio_exists(dst);
    if (existed && !confirm("Overwrite the existing file?", dst))
        return;
    ui_crit_take();
    /* Same drive and nothing in the way: DOS just relinks the entry. */
    if (!existed && rename(name, dst) == 0)
        return;
    rc = sio_copy(name, dst);
    if (rc != SIO_OK) {
        notify("Move failed; nothing was changed.", copy_why(rc));
        return;
    }
    /* The original goes only now that its copy is complete and closed. */
    if (remove(name) != 0)
        notify("Copied, but the original could not be deleted.", name);
}

/* The drive and directory CASTFM was started in.  The current
 * directory belongs to the whole DOS session, not to this program, so
 * the menu and the next tool (CASTMARK writes its results into it)
 * would otherwise inherit wherever the user browsed to. */
static int  start_drive;
static char start_dir[128];

int main(void)
{
    int sel = 0, top = 0, key;
    char dest[80], nm[16];

    start_drive = cur_drive();
    if (getcwd(start_dir, sizeof(start_dir)) == NULL)
        start_dir[0] = '\0';

    ui_init();
    read_dir();

    for (;;) {
        if (sel < 0) sel = 0;
        if (sel >= nent) sel = nent ? nent - 1 : 0;
        if (sel < top) top = sel;
        if (sel >= top + VIS) top = sel - VIS + 1;
        draw(sel, top);
        key = ui_getkey();

        if (key == KEY_F10 || key == KEY_ESC) {
            break;
        } else if (key == KEY_UP) {
            if (sel > 0) sel--;
        } else if (key == KEY_DOWN) {
            if (sel < nent - 1) sel++;
        } else if (key == KEY_PGUP) {
            sel -= VIS; if (sel < 0) sel = 0;
        } else if (key == KEY_PGDN) {
            sel += VIS; if (sel > nent - 1) sel = nent - 1;
        } else if (key == KEY_HOME) {
            sel = 0;
        } else if (key == KEY_END) {
            sel = nent ? nent - 1 : 0;
        } else if (key == KEY_ENTER) {
            if (nent == 0) continue;
            if (ents[sel].is_dir) {
                if (chdir(ents[sel].name) == 0) { read_dir(); sel = 0; top = 0; }
            } else {
                view_file(ents[sel].name);
            }
        } else if (key == KEY_F3) {
            if (nent && !ents[sel].is_dir) view_file(ents[sel].name);
        } else if (key == KEY_F4) {
            if (nent && !ents[sel].is_dir && is_exec(ents[sel].name)) {
                ui_done();
                system(ents[sel].name);
                ui_init();
                read_dir();
            } else {
                notify("Select a program (.EXE/.COM/.BAT) to run.", "");
            }
        } else if (key == KEY_F5) {
            if (nent && !ents[sel].is_dir) {
                dest[0] = '\0';
                if (prompt(" Copy to (path or name) ", dest, sizeof(dest))
                        && dest[0]) {
                    copy_file(ents[sel].name, dest);
                    read_dir();
                }
            }
        } else if (key == KEY_F6) {
            if (nent && !ents[sel].is_dir) {
                strcpy(dest, "");
                if (prompt(" Move to (path or name) ", dest, sizeof(dest))
                        && dest[0]) {
                    move_file(ents[sel].name, dest);
                    read_dir();
                }
            }
        } else if (key == KEY_F7) {
            nm[0] = '\0';
            if (prompt(" New directory name ", nm, sizeof(nm)) && nm[0]) {
                if (make_dir(nm) == 0) read_dir();
                else notify("Could not create the directory.", nm);
            }
        } else if (key == KEY_F8) {
            if (nent && !ents[sel].is_dir) {
                if (confirm("Delete this file?", ents[sel].name)) {
                    if (remove(ents[sel].name) == 0) read_dir();
                    else notify("Delete failed.", ents[sel].name);
                }
            } else if (nent && ents[sel].is_dir) {
                notify("Refusing to delete a directory.",
                       "Use it from the command line if you must.");
            }
        } else if (key == KEY_F9) {
            nm[0] = '\0';
            if (prompt(" Drive letter (A-Z) ", nm, 3) && nm[0]) {
                set_drive(up(nm[0]));
                read_dir(); sel = 0; top = 0;
            }
        }
    }

    /* getcwd() includes the drive, so chdir() resets that drive's
     * directory whichever drive is current. */
    set_drive(start_drive);
    if (start_dir[0])
        chdir(start_dir);

    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
