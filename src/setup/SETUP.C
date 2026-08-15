/* ===================================================================
 * SETUP.C  -  CASTALIA DOS Installer  (SETUP.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Installs CASTALIA DOS from the boot/install media to a hard disk or
 * CompactFlash card.  A text-mode wizard framed by two full-screen
 * splashes that wear the fortress keep:
 *
 *   Welcome splash -> Target -> Options -> Confirm -> Install -> Done splash
 *
 * SAFETY MODEL (important)
 * ------------------------
 * This installer NEVER partitions or formats a disk on its own - that is
 * far too dangerous for an automated tool.  It assumes the target is an
 * already-formatted FAT16 disk (a fresh FORMAT, or an existing DOS).  It:
 *   1. backs up any existing CONFIG.SYS / AUTOEXEC.BAT first,
 *   2. creates the Castalia directory tree,
 *   3. copies the DOS core and Castalia layer from the media (XCOPY),
 *   4. writes the boot files and a sound profile,
 *   5. (optionally, with confirmation) makes the disk bootable via SYS.
 * The one destructive family of operations (SYS / overwriting root config)
 * happens only after an explicit confirmation screen, and after the old
 * config is safely backed up.  Partition/format is left to the user with
 * FDISK / FORMAT (see docs/INSTALL.md).
 *
 * Bulk copy and boot-sector work are delegated to the standard FreeDOS
 * tools (XCOPY, SYS) via system(); the safety logic lives here.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os setup.c ..\common\ui.c ..\common\logo.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include "../common/UI.H"
#include "../common/LOGO.H"

#define KEEP_X ((SCR_W - LOGO_KEEP_W) / 2)

/* Wizard steps. */
enum { STEP_WELCOME, STEP_TARGET, STEP_OPTIONS, STEP_CONFIRM, STEP_DONE };

/* Sound profiles (mirrors SETSOUND). */
static const char *snd_id[]    = { "NONE","SPKR","ADLIB","SB","SBPRO","SB16" };
static const char *snd_label[] = { "None","PC Speaker","AdLib",
                                   "Sound Blaster 2.0","Sound Blaster Pro",
                                   "Sound Blaster 16" };
static const int   snd_type[]  = { 0, 0, 0, 3, 4, 6 };
static const int   snd_hdma[]  = { 0, 0, 0, 0, 0, 1 };
#define NSND 6

static const char *mouse_label[] = { "Serial (COM1)", "PS/2", "None" };
#define NMOUSE 3

static const char *src_opt[] = { "A:\\", "B:\\", ".\\" };
#define NSRC 3

/* Installation choices. */
static char tgt = 'C';
static int  i_src = 0;
static int  o_mouse = 0;
static int  o_sound = 4;      /* SB Pro */
static int  o_cdrom = 1;
static int  o_sys   = 1;      /* make bootable */

/* Install progress bookkeeping (text-mode phase). */
static int g_step, g_total;

/* --- helpers -------------------------------------------------------- */

static const char *src_root(void) { return src_opt[i_src]; }

/* Write the sound include SOUND.BAT on the target for the chosen profile. */
static void write_sound(void)
{
    char path[64], bl[48];
    FILE *fp;
    int s = o_sound;

    sprintf(path, "%c:\\CASTALIA\\CFG\\SOUND.BAT", tgt);
    fp = fopen(path, "w");
    if (fp == NULL)
        return;
    fprintf(fp, "@ECHO OFF\r\n");
    fprintf(fp, "REM Written by CASTALIA SETUP - profile %s\r\n", snd_id[s]);
    if (snd_type[s] != 0) {
        strcpy(bl, "A220 I5 D1");
        if (snd_hdma[s]) strcat(bl, " H5");
        sprintf(bl + strlen(bl), " T%d", snd_type[s]);
        fprintf(fp, "SET BLASTER=%s\r\n", bl);
        fprintf(fp, "SET SOUND=C:\\CASTALIA\r\n");
        fprintf(fp, "SET MIDI=SYNTH:1 MAP:E\r\n");
    } else {
        fprintf(fp, "REM %s: no SET BLASTER needed.\r\n", snd_id[s]);
        fprintf(fp, "SET SOUND=C:\\CASTALIA\r\n");
    }
    fclose(fp);
}

/* --- wizard chrome -------------------------------------------------- */

static void header(const char *right)
{
    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA DOS Setup", A_TITLE);
    ui_puts(SCR_W - (int)strlen(right) - 2, 0, right, UI_ATTR(C_WHITE, C_BLUE));
    ui_hline(0, 1, SCR_W, A_FRAME);
}

static void footer(const char *s)
{
    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(2, SCR_H - 1, s, A_STATUS);
}

/* --- screens -------------------------------------------------------- */

/* Full-screen welcome splash: the keep, the wordmark, and the pitch. */
static void draw_welcome(void)
{
    ui_cls(A_DESKTOP);
    logo_keep(KEEP_X, 0);
    logo_bigtext_center(12, "CASTALIA DOS", UI_ATTR(C_YELLOW, C_BLUE));
    ui_center(17, "S E T U P    -    386SX Edition 1.0  \"Tombatossals\"",
              UI_ATTR(C_WHITE, C_BLUE));
    ui_center(18, "a Tombatossals Softworks product", A_HINT);
    ui_center(19, "Installs CASTALIA DOS on a hard disk or CompactFlash card,",
              A_ITEM);
    ui_center(20, "tuned for real 386-class machines and the games they run.",
              A_ITEM);
    ui_center(21, "It backs up anything it replaces and changes nothing "
              "until you confirm.", A_HINT);
    ui_center(23, "[ Enter ] Continue                    [ Esc ] Exit",
              UI_ATTR(C_LCYAN, C_BLUE));
}

static void draw_target(void)
{
    char line[72];
    header("Step 1 of 3");
    ui_dbox(4, 3, 72, 15, A_FRAME);
    ui_puts(6, 3, " Target and source ", A_TITLE);

    sprintf(line, "Install to drive : %c:", tgt);
    ui_puts(8, 6, line, UI_ATTR(C_YELLOW, C_BLUE));
    ui_puts(40, 6, "(Left/Right change)", A_HINT);

    sprintf(line, "Install from     : %s", src_root());
    ui_puts(8, 8, line, UI_ATTR(C_YELLOW, C_BLUE));
    ui_puts(40, 8, "(PgUp/PgDn change)", A_HINT);

    ui_puts(8, 11, "The source is the install media (usually A:\\).", A_ITEM);
    ui_puts(8, 12, "The target must be a formatted FAT16 disk.  If it is", A_ITEM);
    ui_puts(8, 13, "not, cancel and run FORMAT first (see the manual).", A_ITEM);

    footer(" Left/Right Drive   PgUp/PgDn Source   Enter Next   Esc Back");
}

static void draw_options(int field)
{
    char line[72];
    unsigned char a[4];
    int i;
    header("Step 2 of 3");
    ui_dbox(4, 3, 72, 15, A_FRAME);
    ui_puts(6, 3, " Options ", A_TITLE);

    for (i = 0; i < 4; i++)
        a[i] = (i == field) ? A_ITEMSEL : A_ITEM;

    sprintf(line, " Mouse         : %-18s ", mouse_label[o_mouse]);
    ui_puts(8, 6, line, a[0]);
    sprintf(line, " Sound         : %-18s ", snd_label[o_sound]);
    ui_puts(8, 8, line, a[1]);
    sprintf(line, " CD-ROM support: %-18s ", o_cdrom ? "yes" : "no");
    ui_puts(8, 10, line, a[2]);
    sprintf(line, " Make bootable : %-18s ", o_sys ? "yes (run SYS)" : "no");
    ui_puts(8, 12, line, a[3]);

    ui_puts(8, 15, "CTMOUSE auto-detects serial and PS/2 mice; the choice",
            A_HINT);
    ui_puts(8, 16, "above is a hint only.", A_HINT);

    footer(" Up/Down Field   Left/Right Change   Enter Next   Esc Back");
}

static void draw_confirm(void)
{
    char line[72];
    header("Step 3 of 3");
    ui_dbox(4, 3, 72, 16, A_FRAME);
    ui_puts(6, 3, " Confirm ", A_TITLE);

    sprintf(line, "Target ........ %c:  (FAT16, must already be formatted)", tgt);
    ui_puts(8, 5, line, A_ITEM);
    sprintf(line, "Source ........ %s", src_root());
    ui_puts(8, 6, line, A_ITEM);
    sprintf(line, "Back up ....... %c:\\CONFIG.SYS, %c:\\AUTOEXEC.BAT", tgt, tgt);
    ui_puts(8, 7, line, A_ITEM);
    ui_puts(8, 8, "               -> \\CASTALIA\\BACKUP", A_ITEM);
    ui_puts(8, 9, "Install ....... \\DOS and \\CASTALIA (XCOPY)", A_ITEM);
    sprintf(line, "Mouse %s   Sound %s   CD-ROM %s",
            mouse_label[o_mouse], snd_id[o_sound], o_cdrom ? "yes" : "no");
    ui_puts(8, 10, line, A_ITEM);
    sprintf(line, "Bootable ...... %s", o_sys ? "yes (SYS)" : "no");
    ui_puts(8, 11, line, A_ITEM);

    ui_puts(8, 14, "This will write to disk.  Your data and games are not",
            UI_ATTR(C_YELLOW, C_BLUE));
    sprintf(line, "touched, only %c:\\CONFIG.SYS, %c:\\AUTOEXEC.BAT, \\DOS and "
            "\\CASTALIA.", tgt, tgt);
    ui_puts(8, 15, line, UI_ATTR(C_YELLOW, C_BLUE));

    footer(" Enter Install      Esc Back");
}

/* --- install execution (UI down; framed text progress) -------------- */

static void progress_bar(void)
{
    int filled, i, pct;
    if (g_total <= 0) g_total = 1;
    pct    = g_step * 100 / g_total;
    filled = g_step * 30 / g_total;
    printf("  [");
    for (i = 0; i < 30; i++)
        putchar(i < filled ? '#' : '.');
    printf("]  %3d%%\r", pct);
    fflush(stdout);
}

static void step_begin(const char *desc)
{
    printf("  [%2d/%2d]  %-46s", g_step + 1, g_total, desc);
    fflush(stdout);
}

static void step_end(void)
{
    g_step++;
    printf("done\n");
}

static void run_step(const char *desc, const char *cmd)
{
    step_begin(desc);
    system(cmd);
    step_end();
}

/* Is the target drive actually there, and can we write to it?
 *
 * This exists because of a real 386SX: its hard disk had no partition
 * table at all - sector 0 lacked the 55AA signature, so the kernel
 * printed "illegal partition table - drive 00 sector 0" and there was no
 * C: to install onto.  SETUP charged ahead anyway and failed partway
 * through with a bare "MKDIR failed for C:\CASTALIA\GAMES", which says
 * nothing about the cause and leaves a half-written system behind.
 *
 * INT 21h AH=36h returns FFFFh in AX for an invalid drive letter, which
 * covers "not partitioned" and "no such drive".  A drive can still be
 * present but unformatted or write-protected, so we also try to actually
 * create a file. */
static int drive_ready(char d, const char **why)
{
    union REGS r;
    FILE *fp;
    char probe[20];

    r.h.ah = 0x36;
    r.h.dl = (unsigned char)(d - 'A' + 1);      /* 1 = A:, 3 = C: */
    int86(0x21, &r, &r);
    if (r.x.ax == 0xFFFF) {
        *why = "no such drive - the disk has no DOS partition";
        return 0;
    }

    sprintf(probe, "%c:\\CASTSETU.TMP", d);
    fp = fopen(probe, "wb");
    if (fp == NULL) {
        *why = "cannot write to it - unformatted or write-protected";
        return 0;
    }
    fclose(fp);
    remove(probe);
    return 1;
}

/* Explain the remedy instead of failing halfway through. */
static void no_target_panel(const char *why)
{
    int w = 66, h = 16, x = (SCR_W - w) / 2, y = 4;
    char line[80];

    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " Cannot install yet ", A_PANELHDR);

    sprintf(line, "Drive %c: is not ready to receive Castalia DOS.", tgt);
    ui_putlim(x + 3, y + 2, line, w - 6, A_PANEL);
    sprintf(line, "Reason: %s.", why);
    ui_putlim(x + 3, y + 3, line, w - 6, UI_ATTR(C_RED, C_LGRAY));

    ui_putlim(x + 3, y + 5, "If the kernel printed \"illegal partition table\" while",
              w - 6, A_PANEL);
    ui_putlim(x + 3, y + 6, "booting, the disk has never been partitioned.  Prepare it",
              w - 6, A_PANEL);
    ui_putlim(x + 3, y + 7, "from this floppy, in this order:", w - 6, A_PANEL);

    ui_putlim(x + 5, y + 9,  "1.  FDISK          create a primary DOS partition,",
              w - 8, UI_ATTR(C_BLACK, C_LGRAY));
    ui_putlim(x + 5, y + 10, "                   mark it active, then reboot",
              w - 8, UI_ATTR(C_DGRAY, C_LGRAY));
    sprintf(line, "2.  FORMAT %c: /S   format it and make it bootable", tgt);
    ui_putlim(x + 5, y + 11, line, w - 8, UI_ATTR(C_BLACK, C_LGRAY));
    ui_putlim(x + 5, y + 12, "3.  SETUP          run this installer again",
              w - 8, UI_ATTR(C_BLACK, C_LGRAY));

    ui_puts(x + 3, y + h - 2, "Nothing has been written.  Press any key.",
            UI_ATTR(C_DGRAY, C_LGRAY));
    (void)ui_getkey();
}

static void do_install(void)
{
    char cmd[160];
    const char *s = src_root();

    g_step  = 0;
    g_total = o_sys ? 14 : 13;

    ui_done();
    printf("\n");
    printf("  +============================================================+\n");
    printf("  |   CASTALIA DOS Setup   -   installing to %c:                 |\n",
           tgt);
    printf("  +============================================================+\n\n");

    /* 1. Create the tree (parents first). */
    sprintf(cmd, "IF NOT EXIST %c:\\CASTALIA\\NUL MKDIR %c:\\CASTALIA", tgt, tgt);
    run_step("Creating C:\\CASTALIA", cmd);
    {
        static const char *sub[] =
            { "BIN","DRV","CFG","HELP","GAMES","TOOLS","BACKUP" };
        int i;
        step_begin("Creating the Castalia subfolders");
        for (i = 0; i < 7; i++) {
            sprintf(cmd, "IF NOT EXIST %c:\\CASTALIA\\%s\\NUL "
                    "MKDIR %c:\\CASTALIA\\%s", tgt, sub[i], tgt, sub[i]);
            system(cmd);
        }
        step_end();
    }
    sprintf(cmd, "IF NOT EXIST %c:\\DOS\\NUL MKDIR %c:\\DOS", tgt, tgt);
    run_step("Creating C:\\DOS", cmd);
    sprintf(cmd, "IF NOT EXIST %c:\\GAMES\\NUL MKDIR %c:\\GAMES", tgt, tgt);
    run_step("Creating C:\\GAMES", cmd);

    /* 2. Back up any existing config BEFORE overwriting the root. */
    sprintf(cmd, "IF EXIST %c:\\CONFIG.SYS COPY /Y %c:\\CONFIG.SYS "
            "%c:\\CASTALIA\\BACKUP\\CONFIG.SYS >NUL", tgt, tgt, tgt);
    run_step("Backing up existing CONFIG.SYS", cmd);
    sprintf(cmd, "IF EXIST %c:\\AUTOEXEC.BAT COPY /Y %c:\\AUTOEXEC.BAT "
            "%c:\\CASTALIA\\BACKUP\\AUTOEXEC.BAT >NUL", tgt, tgt, tgt);
    run_step("Backing up existing AUTOEXEC.BAT", cmd);

    /* 3. Copy the DOS core and Castalia layer from the media. */
    sprintf(cmd, "XCOPY %sDOS %c:\\DOS /S /E /Y >NUL", s, tgt);
    run_step("Copying the DOS core", cmd);
    sprintf(cmd, "XCOPY %sCASTALIA %c:\\CASTALIA /S /E /Y >NUL", s, tgt);
    run_step("Copying the Castalia tools", cmd);

    /* 4. Boot files + root config. */
    sprintf(cmd, "IF EXIST %sKERNEL.SYS COPY /Y %sKERNEL.SYS %c:\\ >NUL",
            s, s, tgt);
    run_step("Copying KERNEL.SYS", cmd);
    sprintf(cmd, "IF EXIST %sCOMMAND.COM COPY /Y %sCOMMAND.COM %c:\\ >NUL",
            s, s, tgt);
    run_step("Copying COMMAND.COM", cmd);
    /* Prefer the installed-system templates in INSTALL\ (the floppy's
     * own root CONFIG.SYS boots from A: and must not land on C:). */
    sprintf(cmd, "IF EXIST %sINSTALL\\CONFIG.SYS "
            "COPY /Y %sINSTALL\\CONFIG.SYS %c:\\ >NUL", s, s, tgt);
    system(cmd);
    sprintf(cmd, "IF NOT EXIST %sINSTALL\\CONFIG.SYS "
            "COPY /Y %sCONFIG.SYS %c:\\ >NUL", s, s, tgt);
    run_step("Writing CONFIG.SYS", cmd);
    sprintf(cmd, "IF EXIST %sINSTALL\\AUTOEXEC.BAT "
            "COPY /Y %sINSTALL\\AUTOEXEC.BAT %c:\\ >NUL", s, s, tgt);
    system(cmd);
    sprintf(cmd, "IF NOT EXIST %sINSTALL\\AUTOEXEC.BAT "
            "COPY /Y %sAUTOEXEC.BAT %c:\\ >NUL", s, s, tgt);
    run_step("Writing AUTOEXEC.BAT", cmd);

    /* 5. Sound profile. */
    step_begin("Writing the sound profile");
    write_sound();
    step_end();

    /* 6. Optional: make the disk bootable. */
    if (o_sys) {
        sprintf(cmd, "SYS %c: >NUL", tgt);
        run_step("Making the disk bootable (SYS)", cmd);
    }

    printf("\n");
    progress_bar();
    printf("\n\n  Installation finished.  Press ENTER to raise the keep...");
    fflush(stdout);
    getchar();

    ui_init();
}

/* Full-screen "installed" splash: two block wordmarks and the next steps. */
static void draw_done(void)
{
    int bx = 8, by = 13, bw = SCR_W - 16, bh = 11;
    char line[72];

    ui_cls(A_DESKTOP);
    logo_bigtext_center(1, "CASTALIA DOS", UI_ATTR(C_YELLOW, C_BLUE));
    logo_bigtext_center(7, "INSTALLED", UI_ATTR(C_LGREEN, C_BLUE));
    ui_center(12, "a Tombatossals Softworks product", UI_ATTR(C_LGRAY, C_BLUE));

    ui_fill(bx, by, bw, bh, ' ', A_DESKTOP);
    ui_box(bx, by, bw, bh, A_FRAME);
    ui_puts(bx + 2, by, " The keep stands ", A_TITLE);

    sprintf(line, "Target ...... %c:   (bootable: %s)", tgt,
            o_sys ? "yes" : "no - run SYS later");
    ui_puts(bx + 3, by + 1, line, A_ITEM);
    ui_puts(bx + 3, by + 2, "Old config backed up to \\CASTALIA\\BACKUP.",
            A_ITEM);

    ui_puts(bx + 3, by + 4, "Next steps:", UI_ATTR(C_WHITE, C_BLUE));
    ui_puts(bx + 5, by + 5, "1. Remove the install media.", A_ITEM);
    ui_puts(bx + 5, by + 6,
            "2. Reboot.  Choose a memory profile at the Castalia gate.", A_ITEM);
    ui_puts(bx + 5, by + 7, "3. Add games under C:\\GAMES and to GAMES.INI.",
            A_ITEM);
    ui_puts(bx + 3, by + 9,
            "Keep this disk: it is your emergency rescue disk.",
            UI_ATTR(C_LCYAN, C_BLUE));

    ui_center(SCR_H - 1, "[ Enter / Esc ]  Exit Setup", A_HINT);
}

/* --- main ----------------------------------------------------------- */

int main(void)
{
    int step = STEP_WELCOME;
    int field = 0;
    int key;

    ui_init();
    for (;;) {
        switch (step) {
        case STEP_WELCOME: draw_welcome();        break;
        case STEP_TARGET:  draw_target();         break;
        case STEP_OPTIONS: draw_options(field);   break;
        case STEP_CONFIRM: draw_confirm();        break;
        case STEP_DONE:    draw_done();           break;
        }
        key = ui_getkey();

        if (step == STEP_DONE) {
            if (key == KEY_ENTER || key == KEY_ESC)
                break;
            continue;
        }

        if (key == KEY_ESC) {
            if (step == STEP_WELCOME)
                break;                         /* cancel */
            step--;
            field = 0;
            continue;
        }

        switch (step) {
        case STEP_WELCOME:
            if (key == KEY_ENTER) step = STEP_TARGET;
            break;

        case STEP_TARGET:
            if (key == KEY_LEFT  && tgt > 'C') tgt--;
            else if (key == KEY_RIGHT && tgt < 'H') tgt++;
            else if (key == KEY_PGUP) i_src = (i_src + NSRC - 1) % NSRC;
            else if (key == KEY_PGDN) i_src = (i_src + 1) % NSRC;
            else if (key == KEY_ENTER) { step = STEP_OPTIONS; field = 0; }
            break;

        case STEP_OPTIONS:
            if (key == KEY_UP)   field = (field > 0) ? field - 1 : 3;
            else if (key == KEY_DOWN) field = (field < 3) ? field + 1 : 0;
            else if (key == KEY_LEFT || key == KEY_RIGHT) {
                int d = (key == KEY_RIGHT) ? 1 : -1;
                switch (field) {
                case 0: o_mouse = (o_mouse + NMOUSE + d) % NMOUSE; break;
                case 1: o_sound = (o_sound + NSND + d) % NSND;     break;
                case 2: o_cdrom = !o_cdrom;                        break;
                case 3: o_sys   = !o_sys;                          break;
                }
            } else if (key == KEY_ENTER) step = STEP_CONFIRM;
            break;

        case STEP_CONFIRM:
            if (key == KEY_ENTER) {
                const char *why = "";
                if (!drive_ready(tgt, &why)) {
                    no_target_panel(why);   /* nothing written; go back */
                    step = STEP_WELCOME;
                    break;
                }
                do_install();
                step = STEP_DONE;
            }
            break;
        }
    }

    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
