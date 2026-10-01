/* ===================================================================
 * CASTCOPY.C  -  CASTALIA COPY: floppy-to-disk transfer tool
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * A professional, keyboard-first tool for pulling files off diskettes
 * onto the hard disk: browse A:/B:, tag files (Space/Ins, * for all),
 * pick a destination, then copy with live per-file and total progress
 * bars, transfer speed, and an optional read-back VERIFY pass (on by
 * default - the whole point is rescuing data from aging diskettes).
 *
 * Each file is written under a temporary name in the destination
 * directory, verified there, and only then put in place of any older
 * copy, so a read error or verify mismatch never costs the copy that
 * was already on the hard disk, and no truncated file is ever left.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os castcopy.c ..\common\ui.c ..\common\dirw.c
 *
 * C89 only.  No dynamic allocation.
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
#include <unistd.h>         /* host syntax-check only */
#endif
#include "../common/UI.H"
#include "../common/DIRW.H"
#include "../common/SAFEIO.H"

#define MAX_ENT  512
#define NAMELEN  13
#define CHUNK    4096

typedef struct {
    char name[NAMELEN];
    unsigned long size;
    int is_dir;
    int tag;
} FENT;

static FENT ents[MAX_ENT];
static int  nent = 0;
static char curdir[128];
static char dest[80] = "C:\\GAMES";
static int  verify = 1;
static int  src_drive = 'A';
static int  orig_drive;

static char iobuf[CHUNK];
static char vbuf[CHUNK];

/* --- DOS drive helpers ------------------------------------------------ */

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

static int floppy_count(void)
{
    union REGS r;
    int86(0x11, &r, &r);
    if (!(r.x.ax & 0x0001))
        return 0;
    return (int)(((r.x.ax >> 6) & 3) + 1);
}

/* --- Listing ----------------------------------------------------------- */

static int name_cmp(const void *a, const void *b)
{
    const FENT *x = (const FENT *)a;
    const FENT *y = (const FENT *)b;
    if (x->is_dir != y->is_dir)
        return y->is_dir - x->is_dir;
    return strcmp(x->name, y->name);
}

static void read_dir(void)
{
    nent = 0;
    if (getcwd(curdir, sizeof(curdir)) == NULL)
        strcpy(curdir, "?");
    if (dirw_first("*.*") == 0) {
        do {
            const char *nm = dirw_name();
            if (strcmp(nm, ".") == 0)
                continue;
            if (nent < MAX_ENT) {
                strncpy(ents[nent].name, nm, NAMELEN - 1);
                ents[nent].name[NAMELEN - 1] = '\0';
                ents[nent].size = dirw_size();
                ents[nent].is_dir = dirw_isdir();
                ents[nent].tag = 0;
                nent++;
            }
        } while (dirw_next() == 0);
    }
    qsort(ents, (size_t)nent, sizeof(FENT), name_cmp);
}

static void tag_totals(int *files, unsigned long *bytes)
{
    int i;
    *files = 0;
    *bytes = 0;
    for (i = 0; i < nent; i++) {
        if (ents[i].tag) {
            (*files)++;
            *bytes += ents[i].size;
        }
    }
}

/* --- Dialog helpers ----------------------------------------------------- */

static void notify(const char *l1, const char *l2)
{
    int w = 58, h = 6, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    if (l1) ui_putlim(x + 3, y + 2, l1, w - 6, A_PANEL);
    if (l2) ui_putlim(x + 3, y + 3, l2, w - 6, A_PANEL);
    ui_getkey();
}

/* 1 yes, 0 no, 2 all, -1 cancel. */
static int ask_overwrite(const char *name)
{
    int w = 58, h = 7, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2, k;
    char buf[64];
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " File exists ", A_PANELHDR);
    sprintf(buf, "%s already exists at the destination.", name);
    ui_putlim(x + 3, y + 2, buf, w - 6, A_PANEL);
    ui_puts(x + 3, y + 4, "Y overwrite   N skip   A overwrite all   Esc stop",
            A_PANEL);
    for (;;) {
        k = ui_getkey();
        if (k == 'y' || k == 'Y' || k == KEY_ENTER) return 1;
        if (k == 'n' || k == 'N') return 0;
        if (k == 'a' || k == 'A') return 2;
        if (k == KEY_ESC) return -1;
    }
}

static int file_exists(const char *p)
{
    FILE *fp = fopen(p, "rb");
    if (fp == NULL) return 0;
    fclose(fp);
    return 1;
}

/* --- Screen -------------------------------------------------------------- */

#define LIST_Y 5
#define VIS   16

static void draw(int sel, int top)
{
    char line[80], sz[16];
    int row, gi, tf;
    unsigned long tb;
    unsigned char attr;

    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA COPY", A_TITLE);
    ui_puts(17, 0, "Diskette Rescue & Transfer", UI_ATTR(C_WHITE, C_BLUE));

    sprintf(line, "Source: %-38.38s", curdir);
    ui_puts(2, 2, line, UI_ATTR(C_YELLOW, C_BLUE));
    sprintf(line, "Dest:   %-38.38s", dest);
    ui_puts(2, 3, line, UI_ATTR(C_YELLOW, C_BLUE));

    tag_totals(&tf, &tb);
    sprintf(line, "Tagged: %d file(s), %lu KB", tf, tb / 1024UL);
    ui_puts(48, 2, line, UI_ATTR(C_LGREEN, C_BLUE));
    sprintf(line, "Verify: %s", verify ? "ON " : "off");
    ui_puts(48, 3, line, verify ? UI_ATTR(C_LGREEN, C_BLUE) : A_HINT);

    ui_box(0, 4, SCR_W, 19, A_FRAME);
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
        sprintf(line, "%c %-13.13s %12s", ents[gi].tag ? 0x10 : ' ',
                ents[gi].name, sz);
        ui_puts(2, LIST_Y + row, line,
                ents[gi].tag && gi != sel ? UI_ATTR(C_LGREEN, C_BLUE) : attr);
    }

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(0, SCR_H - 1,
        " Space Tag  * All  Enter Open  A/B Drive  F4 Dest  V Verify  "
        "F5 COPY  Esc Quit", A_STATUS);
}

/* --- Copy engine ----------------------------------------------------------- */

static void join_path(char *out, const char *dir, const char *name)
{
    int n = (int)strlen(dir);
    strcpy(out, dir);
    if (n > 0 && out[n - 1] != '\\')
        strcat(out, "\\");
    strcat(out, name);
}

/* Copy with progress.  Returns 0 ok, -1 read, -2 write, -3 verify,
 * -5 the verified copy could not replace the old one.
 *
 * The data goes to a temporary file next to 'dname'.  Opening 'dname'
 * itself would empty an existing copy before the diskette had given up
 * a byte, and a bad sector later on would then lose both. */
static int copy_one(const char *sname, const char *dname,
                    unsigned long fsize, unsigned long *done_total,
                    unsigned long grand_total, unsigned long t0)
{
    FILE *in, *out;
    size_t n;
    unsigned long done = 0;
    long el;
    char buf[64];
    char tmp[SIO_PATH];

    if (sio_tmpname(dname, tmp) != SIO_OK)
        return -2;
    in = fopen(sname, "rb");
    if (in == NULL)
        return -1;
    out = fopen(tmp, "wb");
    if (out == NULL) {
        fclose(in);
        return -2;
    }
    while ((n = fread(iobuf, 1, CHUNK, in)) > 0) {
        if (fwrite(iobuf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            remove(tmp);
            return -2;
        }
        done += (unsigned long)n;
        *done_total += (unsigned long)n;

        if (fsize > 0)
            ui_hbar(4, 14, 72, (int)((done >> 4) * 1000UL /
                    ((fsize >> 4) ? (fsize >> 4) : 1)), A_TITLE, A_HINT);
        if (grand_total > 0)
            ui_hbar(4, 17, 72, (int)((*done_total >> 4) * 1000UL /
                    ((grand_total >> 4) ? (grand_total >> 4) : 1)),
                    UI_ATTR(C_LGREEN, C_BLUE), A_HINT);
        el = (long)(ui_ticks() - t0);
        if (el > 0) {
            sprintf(buf, "%lu KB   %ld KB/s   ",
                    *done_total / 1024UL,
                    (long)((*done_total >> 10) * 182L / (el * 10L)));
            ui_puts(4, 18, buf, A_HINT);
        }
    }
    if (ferror(in)) {
        fclose(in);
        fclose(out);
        remove(tmp);
        return -1;
    }
    fclose(in);
    /* The last buffer only reaches the disk here; a full disk shows up
     * as a failed close, not as a short fwrite. */
    if (sio_close(out) != 0) {
        remove(tmp);
        return -2;
    }

    if (verify) {
        FILE *a = fopen(sname, "rb");
        FILE *b = fopen(tmp, "rb");
        size_t na, nb;
        int bad = 0;
        if (a == NULL || b == NULL) {
            if (a) fclose(a);
            if (b) fclose(b);
            remove(tmp);
            return -3;
        }
        ui_puts(4, 15, "verifying...", A_HINT);
        do {
            na = fread(iobuf, 1, CHUNK, a);
            nb = fread(vbuf, 1, CHUNK, b);
            if (na != nb || memcmp(iobuf, vbuf, na) != 0) {
                bad = 1;
                break;
            }
        } while (na > 0);
        fclose(a);
        fclose(b);
        ui_puts(4, 15, "            ", A_DESKTOP);
        if (bad) {
            remove(tmp);
            return -3;
        }
    }
    /* Only a complete, verified copy replaces what was there. */
    if (sio_replace(tmp, dname) != SIO_OK)
        return -5;
    return 0;
}

static void copy_tagged(int sel)
{
    int i, tf, all = 0, copied = 0, skipped = 0, errors = 0;
    unsigned long tb, done_total = 0, t0;
    char dst[112], buf[112];
    long el;

    tag_totals(&tf, &tb);
    if (tf == 0 && nent > 0 && !ents[sel].is_dir) {
        ents[sel].tag = 1;              /* nothing tagged: copy current */
        tag_totals(&tf, &tb);
    }
    if (tf == 0) {
        notify("Nothing to copy.", "Tag files with Space first.");
        return;
    }

    /* Make sure the destination exists (create one level if needed). */
    join_path(dst, dest, "CCTEST$$.TMP");
    {
        FILE *fp = fopen(dst, "wb");
        if (fp == NULL) {
            dirw_mkdir(dest);
            fp = fopen(dst, "wb");
        }
        if (fp == NULL) {
            notify("Cannot write to the destination:", dest);
            return;
        }
        fclose(fp);
        remove(dst);
    }

    /* Progress panel. */
    ui_fill(4, 11, 72, 9, ' ', A_PANEL);
    ui_box(3, 10, 74, 11, A_PANEL);
    ui_fill(4, 10, 72, 1, ' ', A_PANELHDR);
    ui_puts(5, 10, " Copying ", A_PANELHDR);

    t0 = ui_ticks();
    for (i = 0; i < nent; i++) {
        int rc = 0, crit;
        if (!ents[i].tag || ents[i].is_dir)
            continue;

        sprintf(buf, "%-13.13s  (%lu bytes)          ",
                ents[i].name, ents[i].size);
        ui_puts(5, 12, buf, A_PANEL);
        ui_puts(5, 13, "file:", A_PANEL);
        ui_puts(5, 16, "total:", A_PANEL);

        join_path(dst, dest, ents[i].name);
        ui_crit_take();             /* report only this file's error */
        /* A destination that is the source directory (or reaches it by
         * another spelling) would mean writing over the file being read. */
        if (sio_same(ents[i].name, dst)) {
            rc = -4;
        } else if (!all && file_exists(dst)) {
            int a = ask_overwrite(ents[i].name);
            /* repaint the panel the dialog covered */
            ui_fill(4, 11, 72, 9, ' ', A_PANEL);
            ui_box(3, 10, 74, 11, A_PANEL);
            ui_fill(4, 10, 72, 1, ' ', A_PANELHDR);
            ui_puts(5, 10, " Copying ", A_PANELHDR);
            if (a == -1) break;
            if (a == 0) { skipped++; continue; }
            if (a == 2) all = 1;
        }

        if (rc == 0)
            rc = copy_one(ents[i].name, dst, ents[i].size,
                          &done_total, tb, t0);
        if (rc == 0) {
            copied++;
            ents[i].tag = 0;
        } else {
            errors++;
            crit = ui_crit_take();  /* what the drive said, if anything */
            sprintf(buf, "ERROR on %s: %s%s%s", ents[i].name,
                    (rc == -1) ? "read failed (bad diskette?)" :
                    (rc == -2) ? "write failed (disk full?)" :
                    (rc == -4) ? "source and destination are one file" :
                    (rc == -5) ? "could not replace the old copy" :
                                 "VERIFY MISMATCH - copy discarded",
                    (crit >= 0) ? " - " : "",
                    (crit >= 0) ? ui_crit_text(crit) : "");
            ui_putlim(5, 19, buf, 70, A_WARN);
            ui_puts(5, 18, "Press a key to continue with the rest...",
                    A_PANEL);
            ui_getkey();
            ui_fill(5, 18, 70, 2, ' ', A_PANEL);
        }
    }

    el = (long)(ui_ticks() - t0);
    if (el < 1) el = 1;
    sprintf(buf, "Done: %d copied, %d skipped, %d error(s), avg %ld KB/s",
            copied, skipped, errors,
            (long)((done_total >> 10) * 182L / (el * 10L)));
    ui_puts(5, 18, buf, errors ? A_WARN : A_PANEL);
    ui_puts(5, 19, "Press any key to return.                    ", A_PANEL);
    ui_getkey();
}

/* --- Main -------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    int sel = 0, top = 0, key, i;

    orig_drive = cur_drive();
    if (argc > 1 && argv[1][1] == ':') {
        src_drive = argv[1][0];
        if (src_drive >= 'a') src_drive -= 32;
    }
    set_drive(src_drive);
    if (chdir("\\") != 0) { /* keep going; read_dir reports what it sees */ }
    read_dir();

    ui_init();
    for (;;) {
        if (sel < 0) sel = 0;
        if (sel >= nent) sel = nent ? nent - 1 : 0;
        if (sel < top) top = sel;
        if (sel >= top + VIS) top = sel - VIS + 1;
        draw(sel, top);
        key = ui_getkey();

        if (key == KEY_ESC || key == KEY_F10) {
            break;
        } else if (key == KEY_UP) {
            if (sel > 0) sel--;
        } else if (key == KEY_DOWN) {
            if (sel < nent - 1) sel++;
        } else if (key == KEY_PGUP) {
            sel -= VIS; if (sel < 0) sel = 0;
        } else if (key == KEY_PGDN) {
            sel += VIS; if (sel > nent - 1) sel = nent - 1;
        } else if (key == KEY_SPACE || key == KEY_INS) {
            if (nent && !ents[sel].is_dir) {
                ents[sel].tag = !ents[sel].tag;
                if (sel < nent - 1) sel++;
            }
        } else if (key == '*') {
            for (i = 0; i < nent; i++)
                if (!ents[i].is_dir)
                    ents[i].tag = 1;
        } else if (key == KEY_ENTER) {
            if (nent && ents[sel].is_dir) {
                if (chdir(ents[sel].name) == 0) {
                    read_dir(); sel = 0; top = 0;
                }
            }
        } else if (key == KEY_BKSP) {
            if (chdir("..") == 0) { read_dir(); sel = 0; top = 0; }
        } else if (key == 'a' || key == 'A' || key == 'b' || key == 'B') {
            int d = (key == 'a' || key == 'A') ? 'A' : 'B';
            if (d == 'B' && floppy_count() < 2) {
                notify("The BIOS reports no B: drive.", "");
            } else {
                src_drive = d;
                set_drive(src_drive);
                chdir("\\");
                read_dir(); sel = 0; top = 0;
            }
        } else if (key == KEY_F4) {
            int w = 60, h = 6, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
            ui_fill(x, y, w, h, ' ', A_PANEL);
            ui_box(x, y, w, h, A_PANEL);
            ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
            ui_puts(x + 2, y, " Destination path ", A_PANELHDR);
            ui_editline(dest, sizeof(dest), x + 3, y + 3, w - 6);
        } else if (key == 'v' || key == 'V') {
            verify = !verify;
        } else if (key == KEY_F5) {
            copy_tagged(sel);
        }
    }

    set_drive(orig_drive);
    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
