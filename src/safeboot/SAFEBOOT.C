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
 *     boot to a prompt (backing up the current one first)
 *
 * File copying is done in C so the tool works even when the shell is in
 * a fragile state.  The system drive is assumed to be C:.
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

#define CFG_SYS   "C:\\CONFIG.SYS"
#define CFG_BAT   "C:\\AUTOEXEC.BAT"
#define BAK_SYS   "C:\\CASTALIA\\BACKUP\\CONFIG.SYS"
#define BAK_BAT   "C:\\CASTALIA\\BACKUP\\AUTOEXEC.BAT"

/* Copy one file.  Returns 0 on success, negative on error. */
static int copyfile(const char *src, const char *dst)
{
    FILE *in, *out;
    char buf[2048];
    size_t n;

    in = fopen(src, "rb");
    if (in == NULL)
        return -1;
    out = fopen(dst, "wb");
    if (out == NULL) {
        fclose(in);
        return -2;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -3;
        }
    }
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

/* Write a minimal, guaranteed-bootable configuration. */
static int write_minimal(void)
{
    FILE *fp;

    fp = fopen(CFG_SYS, "w");
    if (fp == NULL)
        return -1;
    fprintf(fp, "REM CASTALIA DOS - minimal safe config (written by SAFEBOOT)\r\n");
    fprintf(fp, "DEVICE=C:\\DOS\\HIMEMX.EXE\r\n");
    fprintf(fp, "DOS=HIGH\r\n");
    fprintf(fp, "FILES=20\r\n");
    fprintf(fp, "BUFFERS=15\r\n");
    fprintf(fp, "LASTDRIVE=M\r\n");
    fprintf(fp, "SHELL=C:\\COMMAND.COM C:\\ /P /E:512\r\n");
    fclose(fp);

    fp = fopen(CFG_BAT, "w");
    if (fp == NULL)
        return -2;
    fprintf(fp, "@ECHO OFF\r\n");
    fprintf(fp, "SET PATH=C:\\CASTALIA\\BIN;C:\\DOS;C:\\\r\n");
    fprintf(fp, "PROMPT $P$G\r\n");
    fprintf(fp, "ECHO CASTALIA DOS - minimal safe configuration active.\r\n");
    fprintf(fp, "ECHO Type CASTALIA for the menu, or restore a full config.\r\n");
    fclose(fp);
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
    yn(file_exists(CFG_SYS), a); yn(file_exists(CFG_BAT), b);
    sprintf(line, "Current : CONFIG.SYS %-8s   AUTOEXEC.BAT %-8s", a, b);
    ui_puts(6, 4, line, A_ITEM);
    yn(file_exists(BAK_SYS), a); yn(file_exists(BAK_BAT), b);
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
    if (!file_exists(BAK_SYS) && !file_exists(BAK_BAT)) {
        report("No backup found in C:\\CASTALIA\\BACKUP.",
               "Use action 2 first, or install with SETUP.");
        return;
    }
    if (!confirm("Restore CONFIG.SYS and AUTOEXEC.BAT from backup?",
                 "This overwrites the current boot files."))
        return;
    r1 = file_exists(BAK_SYS) ? copyfile(BAK_SYS, CFG_SYS) : 0;
    r2 = file_exists(BAK_BAT) ? copyfile(BAK_BAT, CFG_BAT) : 0;
    if (r1 == 0 && r2 == 0)
        report("Restored from backup.", "Reboot for the changes to apply.");
    else
        report("Restore failed (disk write error?).",
               "Check the disk and try again.");
}

static void act_backup(void)
{
    int r1, r2;
    if (!confirm("Save the current CONFIG.SYS and AUTOEXEC.BAT",
                 "to C:\\CASTALIA\\BACKUP?"))
        return;
    r1 = file_exists(CFG_SYS) ? copyfile(CFG_SYS, BAK_SYS) : -9;
    r2 = file_exists(CFG_BAT) ? copyfile(CFG_BAT, BAK_BAT) : -9;
    if (r1 == 0 || r2 == 0)      /* at least one file backed up */
        report("Saved to C:\\CASTALIA\\BACKUP.", "");
    else
        report("Backup failed or nothing to back up.",
               "Ensure C:\\CASTALIA\\BACKUP exists.");
}

static void act_minimal(void)
{
    if (!confirm("Write a minimal safe configuration?",
                 "Your current config is saved to backup first."))
        return;
    /* Save the current config before overwriting it. */
    if (file_exists(CFG_SYS)) copyfile(CFG_SYS, BAK_SYS);
    if (file_exists(CFG_BAT)) copyfile(CFG_BAT, BAK_BAT);
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
