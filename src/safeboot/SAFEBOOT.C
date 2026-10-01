/* ===================================================================
 * SAFEBOOT.C  -  CASTALIA DOS Rescue Tool  (SAFEBOOT.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Run from Safe Mode or the emergency boot floppy to repair a system
 * whose CONFIG.SYS / AUTOEXEC.BAT is broken.  It can:
 *   - restore CONFIG.SYS / AUTOEXEC.BAT from C:\CASTALIA\BACKUP
 *   - save the current CONFIG.SYS / AUTOEXEC.BAT to the backup folder
 *   - write a known-good MINIMAL configuration that is guaranteed to
 *     boot to a prompt (keeping the current one as CONFIG.SAF and
 *     AUTOEXEC.SAF first, never over the backup that restore uses)
 *
 * File copying is done in C so the tool works even when the shell is in
 * a fragile state.  Every file is written under a temporary name and
 * swapped in only when complete, so a failed write never leaves a
 * truncated boot file.  The system drive is assumed to be C:.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os safeboot.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/UI.H"
#include "../common/SAFEIO.H"

#define CFG_SYS   "C:\\CONFIG.SYS"
#define CFG_BAT   "C:\\AUTOEXEC.BAT"
#define BAK_SYS   "C:\\CASTALIA\\BACKUP\\CONFIG.SYS"
#define BAK_BAT   "C:\\CASTALIA\\BACKUP\\AUTOEXEC.BAT"
/* Where "write minimal" keeps the config it replaces.  Separate names,
 * because BAK_* is the known-good copy that restore relies on, and the
 * config being replaced is very likely the broken one. */
#define SAF_SYS   "C:\\CASTALIA\\BACKUP\\CONFIG.SAF"
#define SAF_BAT   "C:\\CASTALIA\\BACKUP\\AUTOEXEC.SAF"

/* First line of every file write_minimal() produces. */
#define MIN_MARK  "REM CASTALIA DOS - minimal safe config (written by SAFEBOOT)"

/* 1 if 'path' is a config written by write_minimal(): there is nothing
 * worth keeping in it, and copying it over CONFIG.SAF would replace the
 * user's own config saved there by the previous run. */
static int is_minimal(const char *path)
{
    char line[80];
    int n, hit = 0;
    FILE *fp = fopen(path, "r");

    if (fp == NULL)
        return 0;
    for (n = 0; n < 3 && !hit && fgets(line, (int)sizeof(line), fp); n++)
        hit = (strncmp(line, MIN_MARK, strlen(MIN_MARK)) == 0);
    fclose(fp);
    return hit;
}

/* Write one file of the minimal config through a temporary file.
 * 'text' is the whole file, CR LF line ends included. */
static int write_one(const char *path, const char *text)
{
    char tmp[SIO_PATH];
    FILE *fp;

    if (sio_tmpname(path, tmp) != SIO_OK)
        return -1;
    /* Binary mode: the text carries its own CR LF, and text mode would
     * turn each "\r\n" into CR CR LF. */
    fp = fopen(tmp, "wb");
    if (fp == NULL)
        return -1;
    fputs(text, fp);
    if (sio_close(fp) != 0) {
        remove(tmp);
        return -1;
    }
    return sio_replace(tmp, path) == SIO_OK ? 0 : -1;
}

/* Write a minimal, guaranteed-bootable configuration. */
static int write_minimal(void)
{
    if (write_one(CFG_SYS,
            MIN_MARK "\r\n"
            "DEVICE=C:\\DOS\\HIMEMX.EXE\r\n"
            "DOS=HIGH\r\n"
            "FILES=20\r\n"
            "BUFFERS=15\r\n"
            "LASTDRIVE=M\r\n"
            "SHELL=C:\\COMMAND.COM C:\\ /P /E:512\r\n") != 0)
        return -1;
    if (write_one(CFG_BAT,
            "@ECHO OFF\r\n"
            MIN_MARK "\r\n"
            "SET PATH=C:\\CASTALIA\\BIN;C:\\DOS;C:\\\r\n"
            "PROMPT $P$G\r\n"
            "ECHO CASTALIA DOS - minimal safe configuration active.\r\n"
            "ECHO Type CASTALIA for the menu, or restore a full config.\r\n")
            != 0)
        return -2;
    return 0;
}

/* --- UI ------------------------------------------------------------- */

static const char *items[] = {
    "1   Restore CONFIG.SYS & AUTOEXEC.BAT from backup",
    "2   Save current CONFIG.SYS & AUTOEXEC.BAT to backup",
    "3   Write a minimal safe configuration",
    "4   Exit"
};
#define NITEMS 4

static void yn(int ok, char *out)
{
    strcpy(out, ok ? "present" : "MISSING");
}

static void draw_screen(int sel)
{
    char line[72], a[16], b[16];
    int i;
    unsigned char attr;

    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA DOS", A_TITLE);
    ui_puts(SCR_W - 15, 0, "Rescue Tool", UI_ATTR(C_WHITE, C_BLUE));

    /* Status panel. */
    ui_box(4, 2, 72, 7, A_FRAME);
    ui_puts(6, 2, " Status ", A_TITLE);
    yn(sio_exists(CFG_SYS), a); yn(sio_exists(CFG_BAT), b);
    sprintf(line, "Current : CONFIG.SYS %-8s   AUTOEXEC.BAT %-8s", a, b);
    ui_puts(6, 4, line, A_ITEM);
    yn(sio_exists(BAK_SYS), a); yn(sio_exists(BAK_BAT), b);
    sprintf(line, "Backup  : CONFIG.SYS %-8s   AUTOEXEC.BAT %-8s", a, b);
    ui_puts(6, 5, line, A_ITEM);
    ui_puts(6, 7, "Backups live in C:\\CASTALIA\\BACKUP.", A_HINT);

    /* Menu. */
    ui_box(4, 10, 72, NITEMS + 2, A_FRAME);
    ui_puts(6, 10, " Actions ", A_TITLE);
    for (i = 0; i < NITEMS; i++) {
        attr = (i == sel) ? A_ITEMSEL : A_ITEM;
        ui_fill(5, 11 + i, 70, 1, ' ', attr);
        sprintf(line, " %-68.68s", items[i]);
        ui_puts(5, 11 + i, line, attr);
    }

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(2, SCR_H - 1,
        " Up/Down Select   Enter Do   Esc Exit", A_STATUS);
}

/* Confirm dialog: returns 1 for yes. */
static int confirm(const char *l1, const char *l2)
{
    int w = 56, h = 8, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2, k;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " Confirm ", A_PANELHDR);
    if (l1) ui_putlim(x + 3, y + 2, l1, w - 6, A_PANEL);
    if (l2) ui_putlim(x + 3, y + 3, l2, w - 6, A_PANEL);
    ui_puts(x + 3, y + 5, "Enter = yes    Esc = no", A_PANEL);
    for (;;) {
        k = ui_getkey();
        if (k == KEY_ENTER) return 1;
        if (k == KEY_ESC)   return 0;
    }
}

static void report(const char *l1, const char *l2)
{
    int w = 56, h = 7, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " SAFEBOOT ", A_PANELHDR);
    if (l1) ui_putlim(x + 3, y + 2, l1, w - 6, A_PANEL);
    if (l2) ui_putlim(x + 3, y + 4, l2, w - 6, A_PANEL);
    ui_getkey();
}

static void act_restore(void)
{
    int r1, r2;
    if (!sio_exists(BAK_SYS) && !sio_exists(BAK_BAT)) {
        report("No backup found in C:\\CASTALIA\\BACKUP.",
               "Use action 2 first, or install with SETUP.");
        return;
    }
    if (!confirm("Restore CONFIG.SYS and AUTOEXEC.BAT from backup?",
                 "This overwrites the current boot files."))
        return;
    r1 = sio_exists(BAK_SYS) ? sio_copy(BAK_SYS, CFG_SYS) : 0;
    r2 = sio_exists(BAK_BAT) ? sio_copy(BAK_BAT, CFG_BAT) : 0;
    if (r1 == 0 && r2 == 0)
        report("Restored from backup.", "Reboot for the changes to apply.");
    else
        report("Restore failed (disk write error?).",
               "Check the disk and try again.");
}

static void act_backup(void)
{
    int r1, r2;
    if (!confirm("Save the current CONFIG.SYS and AUTOEXEC.BAT to",
                 (sio_exists(BAK_SYS) || sio_exists(BAK_BAT)) ?
                 "C:\\CASTALIA\\BACKUP, replacing the backup there?" :
                 "C:\\CASTALIA\\BACKUP?"))
        return;
    r1 = sio_exists(CFG_SYS) ? sio_copy(CFG_SYS, BAK_SYS) : -9;
    r2 = sio_exists(CFG_BAT) ? sio_copy(CFG_BAT, BAK_BAT) : -9;
    /* -9 = no such file to save.  Any real failure is reported, even
     * when the other file made it. */
    if ((r1 == 0 || r1 == -9) && (r2 == 0 || r2 == -9) && (r1 == 0 || r2 == 0))
        report("Saved to C:\\CASTALIA\\BACKUP.", "");
    else if (r1 == -9 && r2 == -9)
        report("Nothing to back up.", "There is no CONFIG.SYS or AUTOEXEC.BAT.");
    else
        report("Backup failed; an old backup there is unchanged.",
               "Is C:\\CASTALIA\\BACKUP there and writable?");
}

static void act_minimal(void)
{
    if (!confirm("Write a minimal safe configuration?",
                 "Current files are kept in BACKUP as *.SAF first."))
        return;
    /* Keep the config being replaced, but under its own name, and stop
     * if that fails: overwriting the only copy of it is not "safe". */
    if ((sio_exists(CFG_SYS) && !is_minimal(CFG_SYS) &&
         sio_copy(CFG_SYS, SAF_SYS) != SIO_OK) ||
        (sio_exists(CFG_BAT) && !is_minimal(CFG_BAT) &&
         sio_copy(CFG_BAT, SAF_BAT) != SIO_OK)) {
        report("Current config not saved; nothing was changed.",
               "Is C:\\CASTALIA\\BACKUP there and writable?");
        return;
    }
    if (write_minimal() == 0)
        report("Minimal safe config written.",
               "Reboot; you will get a plain prompt that boots.");
    else
        report("Could not write the minimal config.",
               "Check that C: is writable.");
}

int main(void)
{
    int sel = 0, key;

    /* Best-effort: make sure the backup folder exists this session. */
    system("IF NOT EXIST C:\\CASTALIA\\BACKUP\\NUL "
           "MKDIR C:\\CASTALIA\\BACKUP >NUL");

    ui_init();
    for (;;) {
        draw_screen(sel);
        key = ui_getkey();
        if (key == KEY_ESC || (key == '4'))
            break;
        else if (key == KEY_UP)
            sel = (sel > 0) ? sel - 1 : NITEMS - 1;
        else if (key == KEY_DOWN)
            sel = (sel < NITEMS - 1) ? sel + 1 : 0;
        else if (key == '1') { sel = 0; act_restore(); }
        else if (key == '2') { sel = 1; act_backup();  }
        else if (key == '3') { sel = 2; act_minimal(); }
        else if (key == KEY_ENTER) {
            if (sel == 0) act_restore();
            else if (sel == 1) act_backup();
            else if (sel == 2) act_minimal();
            else break;
        }
    }
    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
