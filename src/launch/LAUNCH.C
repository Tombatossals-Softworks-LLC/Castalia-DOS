/* ===================================================================
 * LAUNCH.C  -  CASTALIA DOS Game Launcher  (LAUNCH.EXE / GAMEVAULT.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * A text-mode DOS game browser for real 386SX hardware.  It reads game
 * definitions from GAMES.INI, shows a keyboard-driven list with a detail
 * pane, warns about profile/CD mismatches, and launches the chosen game.
 *
 * MEMORY DISCIPLINE
 * -----------------
 * A launcher that stays resident steals conventional memory from the
 * game.  To avoid that, LAUNCH.EXE does NOT run the game itself.  It
 * writes a one-shot batch file (_RUNGAME.BAT) and exits with errorlevel
 * 77.  The calling batch (GAMES.BAT) then runs _RUNGAME.BAT, so the
 * launcher is completely out of memory while the game runs.
 *
 *     LAUNCH.EXE  ->  writes _RUNGAME.BAT, exits 77
 *     GAMES.BAT   ->  IF ERRORLEVEL 77 CALL _RUNGAME.BAT
 *
 * Build (Open Watcom):
 *     wcl -0 -bt=dos -ml -os launch.c ..\common\ini.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/INI.H"
#include "../common/UI.H"

#define MAX_GAMES   64
#define EXIT_QUIT    0
#define EXIT_RUN    77      /* tell GAMES.BAT to run _RUNGAME.BAT */

/* One game entry.  Fixed-size fields, no heap. */
typedef struct {
    char id[24];            /* INI section name          */
    char name[40];
    char path[64];
    char exe[16];
    char args[48];
    char profile[10];
    char sound[10];
    char notes[160];
    int  requires_cd;
    int  mouse;
} GAME;

static GAME games[MAX_GAMES];
static int  ngames = 0;
static char cur_profile[10] = "XMS";

/* The one place GAMES.BAT looks for the launch batch. */
#define RUNBAT "C:\\CASTALIA\\CFG\\_RUNGAME.BAT"

/* --- Small utilities ------------------------------------------------ */

static void copystr(char *dst, const char *src, int size)
{
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, (size_t)(size - 1));
    dst[size - 1] = '\0';
}

static int file_exists(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return 0;
    fclose(fp);
    return 1;
}

/* --- Loading -------------------------------------------------------- */

/* Try a list of candidate paths and open the first that exists. */
static int load_games_file(const char *override)
{
    static const char *cand[] = {
        "C:\\CASTALIA\\CFG\\GAMES.INI",
        "GAMES.INI",
        "config\\GAMES.INI",
        "..\\..\\config\\GAMES.INI"
    };
    int i;
    if (override != NULL && ini_open(override) == INI_OK)
        return 1;
    for (i = 0; i < (int)(sizeof(cand) / sizeof(cand[0])); i++) {
        if (ini_open(cand[i]) == INI_OK)
            return 1;
    }
    return 0;
}

/* Populate games[] from the loaded INI.  A section is a game if it has
 * an "exe" key. */
static void load_games(void)
{
    int i, n;
    const char *sec;
    const char *v;

    ngames = 0;
    n = ini_section_count();
    for (i = 0; i < n && ngames < MAX_GAMES; i++) {
        sec = ini_section_name(i);
        if (sec == NULL)
            continue;
        v = ini_get(sec, "exe");
        if (v == NULL || v[0] == '\0')
            continue;               /* not a game section */

        copystr(games[ngames].id, sec, sizeof(games[0].id));
        copystr(games[ngames].exe, v, sizeof(games[0].exe));
        copystr(games[ngames].name,
                ini_get_def(sec, "name", sec), sizeof(games[0].name));
        copystr(games[ngames].path,
                ini_get_def(sec, "path", ""), sizeof(games[0].path));
        copystr(games[ngames].args,
                ini_get_def(sec, "args", ""), sizeof(games[0].args));
        copystr(games[ngames].profile,
                ini_get_def(sec, "profile", "XMS"), sizeof(games[0].profile));
        copystr(games[ngames].sound,
                ini_get_def(sec, "sound", "NONE"), sizeof(games[0].sound));
        copystr(games[ngames].notes,
                ini_get_def(sec, "notes", ""), sizeof(games[0].notes));
        games[ngames].requires_cd = ini_get_bool(sec, "requires_cd", 0);
        games[ngames].mouse       = ini_get_bool(sec, "mouse", 0);
        ngames++;
    }
}

/* --- Screen layout -------------------------------------------------- */

#define LIST_X    2
#define LIST_Y    2
#define LIST_W    34
#define LIST_H    20
#define LIST_ROWS (LIST_H - 3)      /* interior rows for names */

#define DET_X     37
#define DET_Y     2
#define DET_W     41
#define DET_H     20

static void draw_frame(void)
{
    char right[40];

    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA DOS", A_TITLE);
    ui_puts(15, 0, "Game Launcher", UI_ATTR(C_WHITE, C_BLUE));

    sprintf(right, "Profile: %s", cur_profile);
    ui_puts(SCR_W - (int)strlen(right) - 2, 0, right, A_TITLE);

    ui_box(LIST_X, LIST_Y, LIST_W, LIST_H, A_FRAME);
    ui_puts(LIST_X + 2, LIST_Y, " Games ", A_TITLE);

    ui_box(DET_X, DET_Y, DET_W, DET_H, A_FRAME);
    ui_puts(DET_X + 2, DET_Y, " Details ", A_TITLE);

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(1, SCR_H - 1,
        " " "\x18\x19" " Move   Enter Launch   E Edit   F1 Help   Esc Quit",
        A_STATUS);
}

static void draw_list(int sel, int top)
{
    int row, gi;
    unsigned char attr;
    char line[LIST_W];
    int inner = LIST_W - 2;

    for (row = 0; row < LIST_ROWS; row++) {
        gi = top + row;
        /* Clear the row first. */
        ui_fill(LIST_X + 1, LIST_Y + 1 + row, inner, 1, ' ', A_ITEM);
        if (gi >= ngames)
            continue;
        attr = (gi == sel) ? A_ITEMSEL : A_ITEM;
        if (gi == sel)
            ui_fill(LIST_X + 1, LIST_Y + 1 + row, inner, 1, ' ', attr);
        sprintf(line, "%-*.*s", inner - 1, inner - 1, games[gi].name);
        ui_puts(LIST_X + 2, LIST_Y + 1 + row, line, attr);
    }

    /* Scroll hints. */
    ui_putc(LIST_X + LIST_W - 2, LIST_Y + 1,
            (char)(top > 0 ? 0x1E : 0x20), A_FRAME);
    ui_putc(LIST_X + LIST_W - 2, LIST_Y + LIST_H - 2,
            (char)((top + LIST_ROWS) < ngames ? 0x1F : 0x20), A_FRAME);
}

/* Print a word-wrapped string inside a rectangle; return lines used. */
static int draw_wrapped(int x, int y, int w, int maxlines,
                        const char *s, unsigned char attr)
{
    char word[64];
    int col = 0, line = 0, i = 0, wl;

    while (s[i] != '\0' && line < maxlines) {
        /* collect one word */
        wl = 0;
        while (s[i] == ' ')
            i++;
        while (s[i] != '\0' && s[i] != ' ' && wl < (int)sizeof(word) - 1)
            word[wl++] = s[i++];
        word[wl] = '\0';
        if (wl == 0)
            continue;
        if (col > 0 && col + 1 + wl > w) {
            col = 0;
            line++;
            if (line >= maxlines)
                break;
        }
        if (col > 0) {
            ui_putc(x + col, y + line, ' ', attr);
            col++;
        }
        ui_putlim(x + col, y + line, word, w - col, attr);
        col += wl;
    }
    return line + 1;
}

static void draw_detail(int sel)
{
    int x = DET_X + 2;
    int y = DET_Y + 2;
    int w = DET_W - 4;
    GAME *g;
    char buf[96];

    /* Clear interior. */
    ui_fill(DET_X + 1, DET_Y + 1, DET_W - 2, DET_H - 2, ' ', A_DESKTOP);
    if (ngames == 0) {
        ui_puts(x, y, "No games defined.", A_HINT);
        ui_puts(x, y + 2, "Edit GAMES.INI or add games with", A_HINT);
        ui_puts(x, y + 3, "the Castalia menu.", A_HINT);
        return;
    }
    g = &games[sel];

    ui_puts(x, y, g->name, UI_ATTR(C_YELLOW, C_BLUE));
    y += 2;

    sprintf(buf, "Path : %s", g->path[0] ? g->path : "(current dir)");
    ui_putlim(x, y++, buf, w, A_ITEM);
    sprintf(buf, "Exe  : %s %s", g->exe, g->args);
    ui_putlim(x, y++, buf, w, A_ITEM);
    sprintf(buf, "Sound: %s", g->sound);
    ui_putlim(x, y++, buf, w, A_ITEM);
    sprintf(buf, "Mouse: %s   CD-ROM: %s",
            g->mouse ? "yes" : "no",
            g->requires_cd ? "required" : "no");
    ui_putlim(x, y++, buf, w, A_ITEM);
    y++;

    /* Recommended profile, coloured by match. */
    {
        unsigned char pa =
            (strcmp(g->profile, cur_profile) == 0)
                ? UI_ATTR(C_LGREEN, C_BLUE)
                : UI_ATTR(C_YELLOW, C_BLUE);
        sprintf(buf, "Recommended profile: %s", g->profile);
        ui_putlim(x, y++, buf, w, pa);
        if (strcmp(g->profile, cur_profile) != 0) {
            sprintf(buf, "(you are booted in %s)", cur_profile);
            ui_putlim(x, y++, buf, w, A_HINT);
        }
    }
    y++;

    if (g->notes[0]) {
        ui_puts(x, y++, "Notes:", UI_ATTR(C_WHITE, C_BLUE));
        draw_wrapped(x, y, w, (DET_Y + DET_H - 2) - y, g->notes,
                     A_ITEM);
    }
}

/* --- Modal helpers -------------------------------------------------- */

static void msgbox(const char *title, const char *l1, const char *l2,
                   const char *l3)
{
    int w = 52, h = 9;
    int x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;

    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, title, A_PANELHDR);
    if (l1) ui_putlim(x + 3, y + 2, l1, w - 6, A_PANEL);
    if (l2) ui_putlim(x + 3, y + 3, l2, w - 6, A_PANEL);
    if (l3) ui_putlim(x + 3, y + 5, l3, w - 6, A_PANEL);
}

/* Confirmation before launch.  Returns 1 to launch, 0 to cancel. */
static int confirm_launch(int sel)
{
    GAME *g = &games[sel];
    char full[96];
    char line2[80];
    int mismatch, cdmiss, key;

    /* Compose full exe path for the existence check. */
    if (g->path[0]) {
        strcpy(full, g->path);
        if (full[strlen(full) - 1] != '\\')
            strcat(full, "\\");
    } else {
        full[0] = '\0';
    }
    strcat(full, g->exe);

    if (!file_exists(full)) {
        msgbox(" Cannot launch ", "The program was not found:", full,
               "Check the path in GAMES.INI.  Press a key.");
        ui_getkey();
        return 0;
    }

    mismatch = (strcmp(g->profile, cur_profile) != 0);
    cdmiss   = (g->requires_cd && strcmp(cur_profile, "CDROM") != 0);

    sprintf(line2, "Launch: %s", g->name);
    if (cdmiss) {
        msgbox(" Warning: CD-ROM profile needed ", line2,
               "This game needs the CD-ROM boot profile.",
               "Enter=launch anyway   Esc=cancel");
    } else if (mismatch) {
        sprintf(full, "Recommended profile %s; you booted %s.",
                g->profile, cur_profile);
        msgbox(" Confirm launch ", line2, full,
               "Enter=launch   R=note reboot   Esc=cancel");
    } else {
        msgbox(" Confirm launch ", line2, "Profile matches.",
               "Enter=launch   Esc=cancel");
    }

    for (;;) {
        key = ui_getkey();
        if (key == KEY_ENTER)
            return 1;
        if (key == KEY_ESC)
            return 0;
        if ((key == 'r' || key == 'R') && mismatch) {
            sprintf(full, "Reboot and pick the %s profile for best results.",
                    g->profile);
            msgbox(" Profile note ", full,
                   "The Castalia menu can help you switch profiles.",
                   "Press a key.");
            ui_getkey();
            return 0;
        }
    }
}

/* --- Batch generation ----------------------------------------------- */

/* Write the one-shot launch batch to RUNBAT.  Returns 1 on success, or
 * 0 with *crit set to the INT 24h code behind the failure (-1 if none).
 *
 * There is no fallback location.  This used to try _RUNGAME.BAT in the
 * current directory next, but GAMES.BAT only ever runs RUNBAT, so the
 * fallback "succeeded", exited 77, launched nothing and left a stray
 * batch behind.  Failing here is the only way the user hears about it.
 * The file is binary because the lines carry their own \r\n; text mode
 * would make every one end \r\r\n. */
static int gen_runbatch(int sel, int *crit)
{
    GAME *g = &games[sel];
    FILE *fp;
    int ok = 1;

    ui_crit_take();
    fp = fopen(RUNBAT, "wb");
    if (fp == NULL) {
        *crit = ui_crit_take();
        return 0;
    }

    if (fprintf(fp, "@ECHO OFF\r\n") < 0) ok = 0;
    if (fprintf(fp, "REM Generated by CASTALIA LAUNCH.EXE - do not keep\r\n") < 0)
        ok = 0;
    /* Change drive if the path names one (e.g. "C:..."). */
    if (g->path[1] == ':') {
        if (fprintf(fp, "%c:\r\n", g->path[0]) < 0) ok = 0;
        if (fprintf(fp, "CD \"%s\"\r\n", g->path + 2) < 0) ok = 0;
    } else if (g->path[0]) {
        if (fprintf(fp, "CD \"%s\"\r\n", g->path) < 0) ok = 0;
    }
    if (g->args[0]) {
        if (fprintf(fp, "%s %s\r\n", g->exe, g->args) < 0) ok = 0;
    } else {
        if (fprintf(fp, "%s\r\n", g->exe) < 0) ok = 0;
    }
    if (fprintf(fp, "C:\r\n") < 0) ok = 0;
    if (fprintf(fp, "CD \\\r\n") < 0) ok = 0;
    if (fclose(fp) != 0) ok = 0;
    if (!ok) {
        /* Leave no half-written batch behind in CFG. */
        *crit = ui_crit_take();
        remove(RUNBAT);
        ui_crit_take();
        return 0;
    }
    return 1;
}

/* --- Help ----------------------------------------------------------- */

static void show_help(void)
{
    int w = 56, h = 14;
    int x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;

    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " Launcher Help ", A_PANELHDR);
    ui_puts(x + 3, y + 2,  "Up / Down .... move selection", A_PANEL);
    ui_puts(x + 3, y + 3,  "PgUp / PgDn .. page the list", A_PANEL);
    ui_puts(x + 3, y + 4,  "Home / End ... first / last game", A_PANEL);
    ui_puts(x + 3, y + 5,  "Enter ........ launch the game", A_PANEL);
    ui_puts(x + 3, y + 6,  "Esc .......... quit to the menu", A_PANEL);
    ui_puts(x + 3, y + 8,  "Games load from GAMES.INI.  The right", A_PANEL);
    ui_puts(x + 3, y + 9,  "pane shows the recommended memory", A_PANEL);
    ui_puts(x + 3, y + 10, "profile and any warnings before you", A_PANEL);
    ui_puts(x + 3, y + 11, "launch.  Press any key to close.", A_PANEL);
    ui_getkey();
}

/* --- Main ----------------------------------------------------------- */

int main(int argc, char *argv[])
{
    int sel = 0, top = 0, key, action = EXIT_QUIT, crit = -1;
    const char *env;
    char why[48];

    env = getenv("CASTPROFILE");
    if (env != NULL && env[0] != '\0')
        copystr(cur_profile, env, sizeof(cur_profile));

    if (!load_games_file(argc > 1 ? argv[1] : NULL)) {
        printf("CASTALIA LAUNCH: GAMES.INI not found.\n");
        printf("Expected at C:\\CASTALIA\\CFG\\GAMES.INI\n");
        return 1;
    }
    load_games();

    ui_init();
    draw_frame();

    for (;;) {
        /* One clamp for every path that moves the bar: with no games,
         * PgDn and a GAMECFG reload both compute ngames-1 = -1, and a
         * negative sel would index games[] and drag top negative too. */
        if (sel > ngames - 1)
            sel = ngames - 1;
        if (sel < 0)
            sel = 0;
        if (sel < top)
            top = sel;
        if (sel >= top + LIST_ROWS)
            top = sel - LIST_ROWS + 1;
        draw_list(sel, top);
        draw_detail(sel);

        key = ui_getkey();
        if (key == KEY_ESC) {
            action = EXIT_QUIT;
            break;
        } else if (key == KEY_UP) {
            if (sel > 0) sel--;
        } else if (key == KEY_DOWN) {
            if (sel < ngames - 1) sel++;
        } else if (key == KEY_PGUP) {
            sel -= LIST_ROWS; if (sel < 0) sel = 0;
        } else if (key == KEY_PGDN) {
            sel += LIST_ROWS; if (sel > ngames - 1) sel = ngames - 1;
        } else if (key == KEY_HOME) {
            sel = 0;
        } else if (key == KEY_END) {
            sel = ngames ? ngames - 1 : 0;
        } else if (key == KEY_F1) {
            show_help();
            draw_frame();
        } else if (key == 'e' || key == 'E') {
            /* Edit the game database, then reload it. */
            if (file_exists("C:\\CASTALIA\\BIN\\GAMECFG.EXE")) {
                ui_done();
                system("C:\\CASTALIA\\BIN\\GAMECFG.EXE");
                ui_init();
                load_games_file(argc > 1 ? argv[1] : NULL);
                load_games();
                if (sel >= ngames) sel = ngames ? ngames - 1 : 0;
                top = 0;
            } else {
                msgbox(" Editor not installed ",
                       "GAMECFG.EXE was not found in", "C:\\CASTALIA\\BIN.",
                       "Press a key.");
                ui_getkey();
            }
            draw_frame();
        } else if (key == KEY_ENTER) {
            if (ngames == 0)
                continue;
            if (confirm_launch(sel)) {
                if (gen_runbatch(sel, &crit)) {
                    action = EXIT_RUN;
                    break;
                } else {
                    if (crit >= 0)
                        sprintf(why, "Reason: %s.", ui_crit_text(crit));
                    else
                        strcpy(why, "Check disk space and try again.");
                    msgbox(" Cannot launch ",
                           "Could not write " RUNBAT, why,
                           "The game was not started.  Press a key.");
                    ui_getkey();
                }
            }
            draw_frame();
        }
    }

    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return action;
}
