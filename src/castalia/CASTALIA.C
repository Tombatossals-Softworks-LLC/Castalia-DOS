/* ===================================================================
 * CASTALIA.C  -  CASTALIA DOS Main Menu v2  (CASTALIA.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * The main text-mode front end, started from AUTOEXEC.BAT unless the
 * user boots the "Command Prompt Only" profile.  Version 2 organises
 * the grown Application Suite into submenus:
 *
 *   1 Launch Games            (batch handoff -> LAUNCH.EXE)
 *   2 File Manager            (CASTFM)
 *   3 System & Benchmark  >   CASTMARK / HWINFO / MEMPROF
 *   4 Disk Tools          >   CASTCOPY / CASTDOC
 *   5 Minigames           >   SNAKE / PUZZLE / ALMENA
 *   6 Sound Setup             (SETSOUND)
 *   7 Configuration       >   CFGEDIT / GAMECFG / backup / restore
 *   8 Help
 *   Q Exit to Command Line
 *
 * Memory discipline: "Launch Games" exits with errorlevel 10 so the
 * AUTOEXEC loop runs the launcher with this menu OUT of memory (real
 * games get every free byte).  The built-in minigames and tools are
 * tiny, so they run in place via system() while the menu waits.
 *
 * Exit codes:  10 = run the game launcher;  0 = exit to the prompt.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os castalia.c ..\common\ini.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include "../common/INI.H"
#include "../common/UI.H"
#include "../common/LOGO.H"

/* Actions. */
enum {
    ACT_LAUNCH,          /* exit 10: AUTOEXEC runs the launcher        */
    ACT_RUN,             /* run 'exe' from C:\CASTALIA\BIN in place    */
    ACT_SUBMENU,         /* open submenu 'sub'                         */
    ACT_BACKUP,
    ACT_RESTORE,
    ACT_HELP,
    ACT_EXIT
};

typedef struct {
    const char *label;
    int         action;
    const char *exe;     /* for ACT_RUN                                */
    int         sub;     /* for ACT_SUBMENU                            */
} ITEM;

/* --- Submenus --------------------------------------------------------- */

static ITEM sub_system[] = {
    { "1  System Benchmark  (CASTMARK)", ACT_RUN, "CASTMARK.EXE", 0 },
    { "2  Hardware Diagnostics",         ACT_RUN, "HWINFO.EXE",   0 },
    { "3  Memory Profiles",              ACT_RUN, "MEMPROF.EXE",  0 },
    { "4  Kernel Signature (CASTID)",    ACT_RUN, "CASTID.EXE",   0 },
    { "5  Screen Saver",                 ACT_RUN, "SAVER.EXE",    0 },
    { "6  Guided Tour",                  ACT_RUN, "CASTTOUR.EXE", 0 }
};
static ITEM sub_disk[] = {
    { "1  Diskette Rescue / Copy",       ACT_RUN, "CASTCOPY.EXE", 0 },
    { "2  Disk Doctor (surface scan)",   ACT_RUN, "CASTDOC.EXE",  0 },
    { "3  CD Audio Player",              ACT_RUN, "CDPLAYER.EXE", 0 },
    { "4  Serial Transfer (CASTLINK)",   ACT_RUN, "CASTLINK.EXE", 0 },
    { "5  Undelete (safe copy-out)",     ACT_RUN, "UNDEL.EXE",    0 }
};
static ITEM sub_games[] = {
    { "1  Snake",                        ACT_RUN, "SNAKE.EXE",    0 },
    { "2  15-Puzzle",                    ACT_RUN, "PUZZLE.EXE",   0 },
    { "3  Almena (falling blocks)",      ACT_RUN, "ALMENA.EXE",   0 },
    { "4  Minas (minesweeper)",          ACT_RUN, "MINAS.EXE",    0 },
    { "5  Siege (catapult duel)",        ACT_RUN, "SIEGE.EXE",    0 },
    { "6  Reversi (Othello)",            ACT_RUN, "REVERSI.EXE",  0 },
    { "7  Barrels (Sokoban)",            ACT_RUN, "BARRELS.EXE",  0 },
    { "8  Solitaire (Klondike)",         ACT_RUN, "SOLITARE.EXE", 0 }
};
static ITEM sub_config[] = {
    { "1  Edit CONFIG.SYS / AUTOEXEC",   ACT_RUN, "CFGEDIT.EXE",  0 },
    { "2  Game Database Editor",         ACT_RUN, "GAMECFG.EXE",  0 },
    { "3  Backup System Config",         ACT_BACKUP,  NULL,       0 },
    { "4  Restore System Config",        ACT_RESTORE, NULL,       0 }
};

typedef struct {
    const char *title;
    ITEM       *items;
    int         count;
} SUBMENU;

/* Counts are derived from the arrays so they can never drift out of sync
 * as entries are added. */
static SUBMENU submenus[] = {
    { " System & Benchmark ", sub_system, (int)(sizeof(sub_system) / sizeof(sub_system[0])) },
    { " Disk Tools ",         sub_disk,   (int)(sizeof(sub_disk)   / sizeof(sub_disk[0])) },
    { " Minigames ",          sub_games,  (int)(sizeof(sub_games)  / sizeof(sub_games[0])) },
    { " Configuration ",      sub_config, (int)(sizeof(sub_config) / sizeof(sub_config[0])) }
};

/* --- Main menu ---------------------------------------------------------- */

static ITEM menu[] = {
    { "1   Launch Games",               ACT_LAUNCH,  NULL,          0 },
    { "2   File Manager",               ACT_RUN,     "CASTFM.EXE",  0 },
    { "3   Text Editor",                ACT_RUN,     "CASTEDIT.EXE", 0 },
    { "4   System & Benchmark      \x10", ACT_SUBMENU, NULL,        0 },
    { "5   Disk Tools               \x10", ACT_SUBMENU, NULL,       1 },
    { "6   Minigames                \x10", ACT_SUBMENU, NULL,       2 },
    { "7   Sound Setup",                ACT_RUN,     "SETSOUND.EXE", 0 },
    { "8   Configuration            \x10", ACT_SUBMENU, NULL,       3 },
    { "9   Help",                       ACT_HELP,    NULL,          0 },
    { "Q   Exit to Command Line",       ACT_EXIT,    NULL,          0 }
};
#define N_ITEMS (int)(sizeof(menu) / sizeof(menu[0]))

/* About info (overridable from CASTALIA.INI [about]). */
static char ab_edition[40]  = "386SX Edition";
static char ab_version[16]  = "1.0";
static char ab_codename[24] = "Tombatossals";
static char cur_profile[10] = "XMS";

/* Ask the kernel which profile actually booted.  The Castalia kernel
 * records the CASTALIA= directive from CONFIG.SYS and hands it back
 * through INT 2Fh AX=CA01h (docs/KERNEL.md); a stock kernel ignores the
 * call, which is why this returns 0 rather than guessing.  Before this
 * existed the menu showed a hard-coded "XMS" whatever had booted - on
 * the rescue floppy, which declares CASTALIA=7, it read XMS while the
 * kernel knew perfectly well it was SAFE. */
static int kernel_profile(void)
{
    union REGS r;
    r.x.ax = 0xCA01;
    int86(0x2F, &r, &r);
    if (r.h.al != 0xFF || r.x.bx != 0xCA5A)
        return 0;
    return (int)r.h.cl;
}

static const char *profile_name(int code)
{
    static const char *names[9] = {
        "(unset)", "CLEAN", "XMS", "EMS", "CDROM",
        "WIN3X", "DIAG", "SAFE", "PROMPT"
    };
    if (code < 1 || code > 8)
        return names[0];
    return names[code];
}

/* --- Utilities ------------------------------------------------------------ */

static void copystr(char *dst, const char *src, int size)
{
    if (src == NULL) { dst[0] = '\0'; return; }
    strncpy(dst, src, (size_t)(size - 1));
    dst[size - 1] = '\0';
}

static int file_exists(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return 0;
    fclose(fp);
    return 1;
}

/* Conventional memory in KB (BIOS INT 12h) - for the About readout. */
static unsigned conv_kb(void)
{
    union REGS r;
    int86(0x12, &r, &r);
    return r.x.ax;
}

/* XMS driver present?  INT 2Fh AX=4300h returns AL=80h when HIMEM (or an
 * equivalent) is loaded.  Same safe probe HWINFO uses. */
static int xms_present(void)
{
    union REGS r;
    r.x.ax = 0x4300;
    int86(0x2F, &r, &r);
    return (r.h.al == 0x80);
}

/* CASTALIA kernel identity via INT 2Fh AX=CA00h (see docs/KERNEL.md).
 * Returns the Castalia kernel build number (>=1) when the CASTALIA kernel
 * answers with its signature word in BX; 0 on a stock kernel.  The About
 * screen uses this to prove the running kernel is genuinely Castalia's. */
static int castalia_kernel(int *oem)
{
    union REGS r;
    r.x.ax = 0xCA00;
    int86(0x2F, &r, &r);
    if (r.h.al != 0xFF || r.x.bx != 0xCA5A)
        return 0;
    if (oem) *oem = r.h.dl;
    return r.x.cx;
}

static void load_about(void)
{
    static const char *cand[] = {
        "C:\\CASTALIA\\CFG\\CASTALIA.INI",
        "CASTALIA.INI",
        "config\\CASTALIA.INI"
    };
    int i;
    const char *env;
    int kp = kernel_profile();

    /* The kernel is the authority: it was told at boot.  CASTPROFILE
     * stays as the fallback for a stock kernel that cannot answer. */
    if (kp >= 1 && kp <= 8) {
        copystr(cur_profile, profile_name(kp), sizeof(cur_profile));
    } else {
        env = getenv("CASTPROFILE");
        if (env != NULL && env[0] != '\0')
            copystr(cur_profile, env, sizeof(cur_profile));
    }

    for (i = 0; i < (int)(sizeof(cand) / sizeof(cand[0])); i++) {
        if (ini_open(cand[i]) == INI_OK) {
            copystr(ab_edition,  ini_get_def("about", "edition",  ab_edition),
                    sizeof(ab_edition));
            copystr(ab_version,  ini_get_def("about", "version",  ab_version),
                    sizeof(ab_version));
            copystr(ab_codename, ini_get_def("about", "codename", ab_codename),
                    sizeof(ab_codename));
            return;
        }
    }
}

/* --- Modal message box ------------------------------------------------------ */

static void msgbox(const char *title, const char *l1, const char *l2,
                   const char *l3)
{
    int w = 54, h = 9;
    int x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, title, A_PANELHDR);
    if (l1) ui_putlim(x + 3, y + 2, l1, w - 6, A_PANEL);
    if (l2) ui_putlim(x + 3, y + 3, l2, w - 6, A_PANEL);
    if (l3) ui_putlim(x + 3, y + 5, l3, w - 6, A_PANEL);
    ui_getkey();
}

/* --- Screen ---------------------------------------------------------------- */

#define MENU_X   22
#define MENU_Y   11
#define MENU_W   36
#define MENU_H   (N_ITEMS + 2)

static void draw_logo(void)
{
    /* The shared compact keep crown, so the menu wears the same face as
     * the boot banner, the rescue disk, and the installer. */
    logo_mark((SCR_W - LOGO_MARK_W) / 2, 1);
}

/* Live wall clock in the top-right corner - the machine feels awake.
 * Read straight from DOS (INT 21h 2Ah/2Ch): whatever the RTC keeps is
 * what we show, with no timezone maths to get wrong.  Layout is fixed
 * width ("Www YYYY-MM-DD" + "  " + "HH:MM:SS" = 24) so it never smears. */
#define CLOCK_W 24
#define CLOCK_X (SCR_W - 1 - CLOCK_W)

static void draw_clock(void)
{
    static const char *dow[7] = { "Sun", "Mon", "Tue", "Wed",
                                  "Thu", "Fri", "Sat" };
    union REGS r;
    int wday, hh, mm, ss;
    char dbuf[16], tbuf[12];

    r.h.ah = 0x2A;                        /* get date */
    int86(0x21, &r, &r);
    wday = r.h.al;
    sprintf(dbuf, "%s %04u-%02u-%02u",
            dow[(wday >= 0 && wday <= 6) ? wday : 0],
            (unsigned)r.x.cx, (unsigned)r.h.dh, (unsigned)r.h.dl);

    r.h.ah = 0x2C;                        /* get time */
    int86(0x21, &r, &r);
    hh = r.h.ch; mm = r.h.cl; ss = r.h.dh;
    sprintf(tbuf, "%02d:%02d:%02d", hh, mm, ss);

    ui_puts(CLOCK_X, 0, dbuf, UI_ATTR(C_WHITE, C_BLUE));   /* date: chrome  */
    ui_puts(CLOCK_X + 16, 0, tbuf, A_TITLE);               /* time: amber   */
}

/* --- The field is alive ---------------------------------------------------
 * A handful of stars over the blue field, plus candlelight in the crown's
 * window and its fluttering pennants.  Everything here is incremental: we
 * only ever repaint the few cells that change, so an idle 386SX menu costs
 * a rounding error.  The star columns deliberately avoid the crown
 * (x 29..50, rows 1..7), the two title lines (8, 9), the menu box
 * (x 22..57, rows 11..22), and the two bars (rows 0 and 24). */
#define NSTAR 14
static const signed char star_x[NSTAR] = {
     3,  8, 14, 19,  5, 11, 17,
    62, 67, 73, 77, 60, 70, 75
};
static const signed char star_y[NSTAR] = {
     3,  6, 10, 15,  18, 21, 13,
     4,  8, 12, 16,  20, 22,  6
};

static void twinkle(void)
{
    static const char glyph[4] = { (char)0xFA, (char)0xF9, '*', (char)0x0F };
    int i = rand() % NSTAR;
    int r = rand() % 100;
    unsigned char a;

    if (r < 55)      a = UI_ATTR(C_DGRAY,  C_BLUE);
    else if (r < 85) a = UI_ATTR(C_LGRAY,  C_BLUE);
    else if (r < 96) a = UI_ATTR(C_WHITE,  C_BLUE);
    else             a = UI_ATTR(C_YELLOW, C_BLUE);

    ui_putc((int)star_x[i], (int)star_y[i], glyph[rand() % 4], a);
}

/* Candlelight in the keep and a flag that will not sit still. */
static void flicker_keep(void)
{
    int mx = (SCR_W - LOGO_MARK_W) / 2;
    int r = rand() % 100;
    unsigned char win, flag;

    if (r < 62)      win = UI_ATTR(C_YELLOW, C_BLUE);   /* lit          */
    else if (r < 90) win = UI_ATTR(C_BROWN,  C_BLUE);   /* guttering    */
    else             win = UI_ATTR(C_DGRAY,  C_BLUE);   /* a draught    */
    logo_mark_windows(mx, 1, win);

    flag = (rand() % 3) ? UI_ATTR(C_YELLOW, C_BLUE)
                        : UI_ATTR(C_LRED,   C_BLUE);
    logo_mark_flags(mx, 1, flag);
}

/* Block until a key is pressed.  While we wait the clock ticks (about
 * twice a second) and the field breathes (about nine times a second).
 * ui_idle() keeps a real CPU (or a DOSBox core) from spinning at 100%. */
static int wait_key(void)
{
    unsigned long last = ui_ticks();
    unsigned long lastfx = ui_ticks();
    for (;;) {
        unsigned long now;
        if (ui_keywaiting())
            return ui_getkey();
        now = ui_ticks();
        if (now - last >= 9UL) {          /* ~0.5 s at 18.2 ticks/s */
            draw_clock();
            last = now;
        }
        if (now - lastfx >= 2UL) {        /* ~9 frames a second     */
            twinkle();
            flicker_keep();
            lastfx = now;
        }
        ui_idle();
    }
}

/* --- Tombatossals ---------------------------------------------------------
 * Type his name at the gate and the giant of Castello folklore - the one
 * who toppled the hills to open the plain, and the codename of 1.0 - rises
 * behind the keep.  Any key sends him back to the mountains.  Nothing is
 * announced anywhere: that is the point of an easter egg. */
static const char SECRET[] = "TOMBA";

static int secret_feed(int key)
{
    static char seen[8];
    int n = (int)(sizeof(SECRET)) - 1;      /* 5 letters, minus the NUL */
    int i;

    if (key < 'A' || (key > 'Z' && key < 'a') || key > 'z')
        return 0;
    if (key >= 'a')
        key -= 32;                          /* fold to upper case       */

    for (i = 1; i < n; i++)                 /* slide the window along   */
        seen[i - 1] = seen[i];
    seen[n - 1] = (char)key;

    for (i = 0; i < n; i++)
        if (seen[i] != SECRET[i])
            return 0;
    return 1;
}

static void tombatossals(void)
{
    static const char *fig[9] = {
        "      .-------.      ",
        "     /  o   o  \\     ",
        "    |     ^     |    ",
        "     \\   ---   /     ",
        "  ,---'-------'---,  ",
        " /   |         |   \\ ",
        "'    |         |    '",
        "     |         |     ",
        "    _|         |_    "
    };
    int w = 21, rows = 9;
    int gx = (SCR_W - w) / 2;
    int base = 10, top, i;
    unsigned long t;

    /* He rises out of the plain, one row per couple of ticks.  The waits
     * count elapsed ticks as an unsigned difference, as wait_key() does:
     * a deadline compared with "<" never arrives once the BIOS counter
     * resets at midnight, and the screen would hang for a day.  A key
     * sends him back early. */
    for (top = 23; top >= base; top--) {
        ui_fill(gx, top + rows, w, 1, ' ', A_DESKTOP);
        for (i = 0; i < rows; i++)
            if (top + i >= 1 && top + i <= SCR_H - 2)
                ui_puts(gx, top + i, fig[i], UI_ATTR(C_BROWN, C_BLUE));
        t = ui_ticks();
        while (ui_ticks() - t < 2UL) {
            if (ui_keywaiting()) {
                (void)ui_getkey();
                return;
            }
            ui_idle();
        }
    }

    ui_center(base + rows + 1, "T O M B A T O S S A L S", A_TITLE);
    ui_center(base + rows + 2,
              "He toppled the hills to open the plain.",
              UI_ATTR(C_WHITE, C_BLUE));

    /* Let him stand there, blinking, until someone looks away. */
    for (;;) {
        if (ui_keywaiting()) {
            (void)ui_getkey();
            break;
        }
        for (i = 0; i < 2; i++)
            twinkle();
        t = ui_ticks();
        while (ui_ticks() - t < 3UL && !ui_keywaiting())
            ui_idle();
    }
}

static void draw_screen(void)
{
    /* Sized for the longest [about] fields CASTALIA.INI can set (39, 15
     * and 23 characters) plus the separators; ui_center clips the rest
     * at the screen edge. */
    char line[96];

    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA DOS", A_TITLE);
    draw_clock();                       /* live date/time, top-right corner */

    draw_logo();

    ui_center(8, "C A S T A L I A   D O S", A_TITLE);
    sprintf(line, "%s   -   %s \"%s\"", ab_edition, ab_version, ab_codename);
    ui_center(9, line, UI_ATTR(C_WHITE, C_BLUE));

    ui_box(MENU_X, MENU_Y, MENU_W, MENU_H, A_FRAME);
    ui_puts(MENU_X + 2, MENU_Y, " Main Menu ", A_TITLE);

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    sprintf(line, " Profile: %s   \x18\x19 Move  Enter Select  F1 About  Esc Exit",
            cur_profile);
    ui_puts(1, SCR_H - 1, line, A_STATUS);
}

static void draw_menu(int sel)
{
    int i;
    unsigned char attr;
    char row[MENU_W];
    int inner = MENU_W - 2;

    for (i = 0; i < N_ITEMS; i++) {
        attr = (i == sel) ? A_ITEMSEL : A_ITEM;
        ui_fill(MENU_X + 1, MENU_Y + 1 + i, inner, 1, ' ', attr);
        sprintf(row, " %-*.*s", inner - 2, inner - 2, menu[i].label);
        ui_puts(MENU_X + 1, MENU_Y + 1 + i, row, attr);
    }
}

/* --- Actions ------------------------------------------------------------------ */

static void run_tool(const char *exe, const char *friendly)
{
    char path[64];
    sprintf(path, "C:\\CASTALIA\\BIN\\%s", exe);
    if (!file_exists(path)) {
        msgbox(" Not installed ", friendly,
               "This tool is not present in this build.",
               "Press a key to return.");
        return;
    }
    ui_done();
    system(path);
    ui_init();
}

static void do_backup(void)
{
    ui_done();
    system("IF NOT EXIST C:\\CASTALIA\\BACKUP\\NUL "
           "MKDIR C:\\CASTALIA\\BACKUP");
    system("COPY /Y C:\\CONFIG.SYS   C:\\CASTALIA\\BACKUP\\CONFIG.SYS  >NUL");
    system("COPY /Y C:\\AUTOEXEC.BAT C:\\CASTALIA\\BACKUP\\AUTOEXEC.BAT >NUL");
    ui_init();
    draw_screen();
    msgbox(" Backup complete ",
           "CONFIG.SYS and AUTOEXEC.BAT were copied to",
           "C:\\CASTALIA\\BACKUP.", "Press a key to return.");
}

static void do_restore(void)
{
    if (!file_exists("C:\\CASTALIA\\BACKUP\\CONFIG.SYS")) {
        msgbox(" Nothing to restore ",
               "No backup was found in C:\\CASTALIA\\BACKUP.",
               "Use Backup System Config first.",
               "Press a key to return.");
        return;
    }
    ui_done();
    system("COPY /Y C:\\CASTALIA\\BACKUP\\CONFIG.SYS   C:\\CONFIG.SYS  >NUL");
    system("COPY /Y C:\\CASTALIA\\BACKUP\\AUTOEXEC.BAT C:\\AUTOEXEC.BAT >NUL");
    ui_init();
    draw_screen();
    msgbox(" Restore complete ",
           "The saved CONFIG.SYS and AUTOEXEC.BAT were",
           "restored.  Reboot for changes to take effect.",
           "Press a key to return.");
}

/* --- Credits roll -------------------------------------------------------------
 * A demoscene-style scrolling credits sequence over the blue field: the
 * wordmark and the names climb the screen while a few stars twinkle above.
 * Reached from the About screen ('C') or by running CASTALIA /CREDITS.  Any
 * key skips out; it also ends on its own when the roll clears the top.
 *
 * The band is rows CR_TOP..CR_BOT.  Each element sits at a virtual row 'v'
 * in a tall column; on screen it lands at CR_TOP + (v - offset), and offset
 * creeps up ~9 rows/second so the whole column drifts upward. */

#define CR_TOP 3
#define CR_BOT 22
#define CR_H   (CR_BOT - CR_TOP + 1)

typedef struct {
    int          v;      /* virtual row (top, for big text)      */
    const char  *t;      /* line text                            */
    unsigned char a;     /* attribute (ignored when big != 0)    */
    int          big;    /* 1 = 5-row block letters (amber)      */
} CREDIT;

static void credits_stars(long offset)
{
    /* A handful of stars above the band; one twinkles white each step. */
    static const int sx[] = { 6, 15, 27, 52, 66, 74 };
    static const int sy[] = { 1,  2,  1,  2,  1,  2 };
    int i, n = (int)(sizeof(sx) / sizeof(sx[0]));
    int lit = (int)(((offset % n) + n) % n);
    for (i = 0; i < n; i++)
        ui_putc(sx[i], sy[i], (char)0xFA,
                (i == lit) ? UI_ATTR(C_WHITE, C_BLUE) : A_HINT);
}

static void credits_roll(void)
{
    static const CREDIT roll[] = {
        {  0, "CASTALIA DOS",                              0,       1 },
        {  6, "386SX Edition   -   1.0  \"Tombatossals\"",
                                          UI_ATTR(C_WHITE, C_BLUE),  0 },
        {  8, "a Tombatossals Softworks production",
                                          UI_ATTR(C_LGRAY, C_BLUE),  0 },
        { 11, "Created by",                               A_TITLE,  0 },
        { 13, "Dave Abellan",             UI_ATTR(C_WHITE, C_BLUE),  0 },
        { 14, "creator & programmer",                     A_HINT,   0 },
        { 16, "Claudio di Castello",      UI_ATTR(C_WHITE, C_BLUE),  0 },
        { 17, "systems, tools & design",                  A_HINT,   0 },
        { 20, "Built on the shoulders of",                A_TITLE,  0 },
        { 22, "FreeDOS  -  the free DOS kernel & shell",
                                          UI_ATTR(C_LGRAY, C_BLUE),  0 },
        { 23, "Open Watcom  -  the C compiler & linker",
                                          UI_ATTR(C_LGRAY, C_BLUE),  0 },
        { 24, "CP437 VGA text - the mode we grew up on",
                                          UI_ATTR(C_LGRAY, C_BLUE),  0 },
        { 27, "For everyone keeping the old machines alive.",
                                          UI_ATTR(C_LGRAY, C_BLUE),  0 },
        { 30, "GRACIAS",                                  0,        1 },
        { 36, "the fortress holds",                       A_HINT,   0 },
        { 38, "(C) 2026 Tombatossals Softworks",          A_HINT,   0 }
    };
    int n = (int)(sizeof(roll) / sizeof(roll[0]));
    long offset = -(long)CR_H;
    unsigned long last = ui_ticks();
    int quit = 0, i, sy;

    ui_cls(A_DESKTOP);
    ui_center(SCR_H - 1, "Press any key to return to the keep.", A_HINT);

    while (!quit) {
        ui_fill(0, CR_TOP, SCR_W, CR_H, ' ', A_DESKTOP);
        credits_stars(offset);
        for (i = 0; i < n; i++) {
            sy = CR_TOP + (int)(roll[i].v - offset);
            if (roll[i].big) {
                if (sy >= CR_TOP && sy + LOGO_BIG_H - 1 <= CR_BOT)
                    logo_bigtext_center(sy, roll[i].t, A_TITLE);
            } else if (sy >= CR_TOP && sy <= CR_BOT) {
                ui_center(sy, roll[i].t, roll[i].a);
            }
        }
        /* Hold each frame ~2 ticks, then climb one row - unless a key skips. */
        for (;;) {
            if (ui_keywaiting()) { ui_getkey(); quit = 1; break; }
            if (ui_ticks() - last >= 2UL) { last = ui_ticks(); offset++; break; }
            ui_idle();
        }
        if (offset > (long)roll[n - 1].v + 3)   /* rolled clear of the top */
            quit = 1;
    }
}

/* The About screen (branding bible section 23.5).  A calm gray panel that
 * states what Castalia is, that it presents MS-DOS 6.22-compatible
 * behavior, and - now that the boot banner is CASTALIA-only - carries the
 * honest FreeDOS/GPL attribution. */
static void about_screen(void)
{
    char v[104];        /* the [about] line can reach 98 characters */
    int w = 68, h = 19;
    int x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
    unsigned char body   = A_PANEL;
    unsigned char accent = UI_ATTR(C_BLUE,  C_LGRAY);
    unsigned char dim    = UI_ATTR(C_DGRAY, C_LGRAY);

    ui_fill(x, y, w, h, ' ', body);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " About CASTALIA DOS ", A_PANELHDR);

    sprintf(v, "CASTALIA DOS %s  -  %s \"%s\"",
            ab_edition, ab_version, ab_codename);
    ui_putlim(x + 3, y + 2, v, w - 5, accent);  /* stay inside the frame */

    /* Live readout of the machine it is actually running on. */
    ui_puts(x + 3, y + 3, "This machine", dim);
    sprintf(v, "%u KB base   -   %s   -   profile %s",
            conv_kb(), xms_present() ? "XMS present" : "no XMS", cur_profile);
    ui_puts(x + 17, y + 3, v, body);

    ui_puts(x + 3, y + 4,
            "A DOS-compatible operating environment for real machines.", body);
    {
        int kbuild, koem = 0;
        kbuild = castalia_kernel(&koem);
        if (kbuild)
            sprintf(v, "Castalia kernel: build %d (OEM %02Xh, MS-DOS 6.22 compatible).",
                    kbuild, koem);
        else
            strcpy(v, "Presents MS-DOS 6.22-compatible behavior.");
        ui_puts(x + 3, y + 5, v, body);
    }

    ui_puts(x + 3, y + 7, "A Tombatossals Softworks product.", accent);
    ui_puts(x + 3,  y + 8, "Created by", dim);
    ui_puts(x + 16, y + 8, "Dave Abellan", body);
    ui_puts(x + 16, y + 9, "Claudio di Castello", body);

    ui_puts(x + 3, y + 12,
            "Built on FreeDOS (GPLv2+) and other free/open software.", body);
    ui_puts(x + 3, y + 13,
            "Castalia code & branding: MIT.  FreeDOS: GPLv2+ (third_party).",
            body);

    ui_puts(x + 3, y + 15, "(C) 2026 Tombatossals Softworks.", body);
    ui_puts(x + 3, y + 17, "Press C for the credits roll  -  any other key returns.",
            dim);
    {
        int k = ui_getkey();
        if (k == 'c' || k == 'C')
            credits_roll();
    }
}

/* Execute a leaf action.  Returns exit code (0/10) or -1 to continue. */
static int do_action(ITEM *it)
{
    switch (it->action) {
    case ACT_LAUNCH:  return 10;
    case ACT_RUN:     run_tool(it->exe, it->label); break;
    case ACT_BACKUP:  do_backup();  break;
    case ACT_RESTORE: do_restore(); break;
    case ACT_HELP:
        /* Full topic reader when installed; About screen as fallback. */
        if (file_exists("C:\\CASTALIA\\BIN\\HELP.EXE"))
            run_tool("HELP.EXE", "Help topics");
        else
            about_screen();
        break;
    case ACT_EXIT:    return 0;
    default:          break;
    }
    return -1;
}

/* --- Submenu loop ---------------------------------------------------------------
 * Pops beside the main menu.  Returns -1 (stay) or an exit code. */

static int run_submenu(int nsub)
{
    SUBMENU *sm = &submenus[nsub];
    int w = 34, h = sm->count + 2;
    int x = MENU_X + MENU_W - 4;
    int y = MENU_Y + 2 + nsub;
    int sel = 0, i, key, rc;
    char row[40];

    if (x + w > SCR_W) x = SCR_W - w - 1;

    for (;;) {
        ui_fill(x, y, w, h, ' ', A_PANEL);
        ui_box(x, y, w, h, A_PANEL);
        ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
        ui_puts(x + 2, y, sm->title, A_PANELHDR);
        for (i = 0; i < sm->count; i++) {
            unsigned char a = (i == sel) ? A_ITEMSEL : A_PANEL;
            ui_fill(x + 1, y + 1 + i, w - 2, 1, ' ', a);
            sprintf(row, " %-*.*s", w - 4, w - 4, sm->items[i].label);
            ui_puts(x + 1, y + 1 + i, row, a);
        }

        key = ui_getkey();
        if (key == KEY_ESC || key == KEY_LEFT)
            return -1;
        else if (key == KEY_UP)
            sel = (sel > 0) ? sel - 1 : sm->count - 1;
        else if (key == KEY_DOWN)
            sel = (sel < sm->count - 1) ? sel + 1 : 0;
        else if (key >= '1' && key < '1' + sm->count) {
            sel = key - '1';
            rc = do_action(&sm->items[sel]);
            if (rc >= 0) return rc;
            draw_screen();
            draw_menu(-1);
        } else if (key == KEY_ENTER) {
            rc = do_action(&sm->items[sel]);
            if (rc >= 0) return rc;
            draw_screen();
            draw_menu(-1);
        }
    }
}

/* Map a main-menu hotkey to an index, or -1. */
static int hotkey_index(int key)
{
    if (key >= '1' && key <= '9')
        return key - '1';
    if (key == 'q' || key == 'Q')
        return N_ITEMS - 1;
    return -1;
}

/* Activate a main-menu entry.  Returns exit code (0/10) or -1. */
static int activate(int idx)
{
    ITEM *it = &menu[idx];
    int rc;

    if (it->action == ACT_SUBMENU) {
        rc = run_submenu(it->sub);
        draw_screen();
        return rc;
    }
    rc = do_action(it);
    if (rc < 0)
        draw_screen();
    return rc;
}

/* --- Main -------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    int sel = 0, key, hi, rc = 0;

    load_about();

    /* CASTALIA /VER : print the one-line version and exit (bible 23.5). */
    if (argc > 1 && (argv[1][0] == '/' || argv[1][0] == '-') &&
        (argv[1][1] == 'v' || argv[1][1] == 'V')) {
        printf("Castalia DOS %s %s (%s)\n",
               ab_edition, ab_version, ab_codename);
        return 0;
    }

    /* CASTALIA /CREDITS : play the animated credits roll and exit. */
    if (argc > 1 && (argv[1][0] == '/' || argv[1][0] == '-') &&
        (argv[1][1] == 'c' || argv[1][1] == 'C')) {
        ui_init();
        credits_roll();
        ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
        ui_done();
        return 0;
    }

    ui_init();
    srand((unsigned)ui_ticks());         /* the stars need somewhere to start */
    draw_screen();

    for (;;) {
        draw_menu(sel);
        key = wait_key();

        if (key == KEY_ESC) {
            rc = 0;
            break;
        } else if (key == KEY_UP) {
            sel = (sel > 0) ? sel - 1 : N_ITEMS - 1;
        } else if (key == KEY_DOWN) {
            sel = (sel < N_ITEMS - 1) ? sel + 1 : 0;
        } else if (key == KEY_HOME) {
            sel = 0;
        } else if (key == KEY_END) {
            sel = N_ITEMS - 1;
        } else if (key == KEY_ENTER || key == KEY_RIGHT) {
            if (key == KEY_RIGHT && menu[sel].action != ACT_SUBMENU)
                continue;
            rc = activate(sel);
            if (rc >= 0)
                break;
        } else if (key == KEY_F1) {
            about_screen();
            draw_screen();
        } else if (secret_feed(key)) {
            tombatossals();
            draw_screen();
        } else {
            hi = hotkey_index(key);
            if (hi >= 0) {
                sel = hi;
                rc = activate(sel);
                if (rc >= 0)
                    break;
            }
        }
    }

    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return rc;
}
