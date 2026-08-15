/* ===================================================================
 * GAMECFG.C  -  CASTALIA DOS Game Config Editor  (GAMECFG.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Add and edit game entries in GAMES.INI without hand-editing the file.
 * It loads every game, lets you edit fields in a form (text fields via an
 * inline editor; profile/sound by cycling; mouse/CD as yes/no), add a new
 * game, or delete one, then writes the whole GAMES.INI back.  It backs the
 * file up to C:\CASTALIA\BACKUP before saving.
 *
 * The launcher (LAUNCH.EXE) reads the same file; this is its editor.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os gamecfg.c ..\common\ini.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/INI.H"
#include "../common/UI.H"

#define MAX_GAMES 64

typedef struct {
    char id[24];
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
static int  dirty = 0;
static char ini_path[80];       /* where GAMES.INI was loaded/saved */

static const char *profiles[] = { "CLEAN", "XMS", "EMS", "CDROM" };
#define NPROF 4
static const char *sounds[] = { "NONE","SPKR","ADLIB","SB","SBPRO","SB16" };
#define NSOUND 6

/* --- small helpers -------------------------------------------------- */

static void copystr(char *dst, const char *src, int size)
{
    if (src == NULL) { dst[0] = '\0'; return; }
    strncpy(dst, src, (size_t)(size - 1));
    dst[size - 1] = '\0';
}

static int file_exists(const char *p)
{
    FILE *fp = fopen(p, "rb");
    if (fp == NULL) return 0;
    fclose(fp);
    return 1;
}

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

/* Uppercase a char (ASCII). */
static char up(char c)
{
    if (c >= 'a' && c <= 'z') return (char)(c - 32);
    return c;
}

/* Index of 'val' in a preset list (case-insensitive), or 0. */
static int enum_index(const char **list, int n, const char *val)
{
    int i, j;
    for (i = 0; i < n; i++) {
        for (j = 0; list[i][j] && val[j]; j++)
            if (up(list[i][j]) != up(val[j])) break;
        if (list[i][j] == '\0' && val[j] == '\0')
            return i;
    }
    return 0;
}

/* --- load / save ---------------------------------------------------- */

static int load_games(void)
{
    static const char *cand[] = {
        "C:\\CASTALIA\\CFG\\GAMES.INI",
        "GAMES.INI",
        "config\\GAMES.INI"
    };
    int i, n;
    const char *sec, *v;

    ini_path[0] = '\0';
    for (i = 0; i < (int)(sizeof(cand) / sizeof(cand[0])); i++)
        if (ini_open(cand[i]) == INI_OK) { strcpy(ini_path, cand[i]); break; }
    if (ini_path[0] == '\0') {
        /* No file yet: start empty, will save to the default path. */
        strcpy(ini_path, "C:\\CASTALIA\\CFG\\GAMES.INI");
        return 1;
    }

    n = ini_section_count();
    for (i = 0; i < n && ngames < MAX_GAMES; i++) {
        sec = ini_section_name(i);
        if (sec == NULL) continue;
        v = ini_get(sec, "exe");
        if (v == NULL || v[0] == '\0') continue;
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
    return 1;
}

/* Make a unique section id from a name (A-Z0-9), excluding game 'self'. */
static void gen_id(GAME *g, int self)
{
    char base[24];
    int i, j = 0, k, suffix;

    for (i = 0; g->name[i] && j < 12; i++) {
        char c = up(g->name[i]);
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            base[j++] = c;
    }
    if (j == 0) { strcpy(base, "GAME"); j = 4; }
    base[j] = '\0';

    for (suffix = 0; suffix < 100; suffix++) {
        char cand[24];
        int clash = 0;
        if (suffix == 0) strcpy(cand, base);
        else sprintf(cand, "%.20s%d", base, suffix);
        for (k = 0; k < ngames; k++) {
            if (k == self) continue;
            if (strcmp(games[k].id, cand) == 0) { clash = 1; break; }
        }
        if (!clash) { copystr(g->id, cand, sizeof(g->id)); return; }
    }
    copystr(g->id, base, sizeof(g->id));
}

static int save_games(void)
{
    static const char *cand[] = {
        "C:\\CASTALIA\\CFG\\GAMES.INI",
        "GAMES.INI"
    };
    FILE *fp;
    int i;
    const char *path = ini_path[0] ? ini_path : cand[0];

    /* Back up the existing file first. */
    if (file_exists(path))
        copyfile(path, "C:\\CASTALIA\\BACKUP\\GAMES.INI");

    fp = fopen(path, "w");
    if (fp == NULL) {
        /* Fall back to a writable location. */
        for (i = 0; i < (int)(sizeof(cand) / sizeof(cand[0])); i++) {
            fp = fopen(cand[i], "w");
            if (fp != NULL) { strcpy(ini_path, cand[i]); break; }
        }
        if (fp == NULL) return -1;
    }

    fprintf(fp, "; CASTALIA DOS game database - edited by GAMECFG.EXE\r\n");
    fprintf(fp, "; One [SECTION] per game; read by LAUNCH.EXE.\r\n\r\n");
    for (i = 0; i < ngames; i++) {
        GAME *g = &games[i];
        fprintf(fp, "[%s]\r\n", g->id);
        fprintf(fp, "name        = %s\r\n", g->name);
        fprintf(fp, "path        = %s\r\n", g->path);
        fprintf(fp, "exe         = %s\r\n", g->exe);
        fprintf(fp, "args        = %s\r\n", g->args);
        fprintf(fp, "profile     = %s\r\n", g->profile);
        fprintf(fp, "sound       = %s\r\n", g->sound);
        fprintf(fp, "mouse       = %s\r\n", g->mouse ? "yes" : "no");
        fprintf(fp, "requires_cd = %s\r\n", g->requires_cd ? "yes" : "no");
        fprintf(fp, "notes       = %s\r\n\r\n", g->notes);
    }
    fclose(fp);
    dirty = 0;
    return 0;
}

/* Single-line editing comes from the shared UI library (ui_editline). */

/* --- edit form ------------------------------------------------------ */

enum { F_NAME, F_PATH, F_EXE, F_ARGS, F_PROFILE, F_SOUND,
       F_MOUSE, F_CD, F_NOTES, NFIELDS };

static const char *flabel[NFIELDS] = {
    "Name", "Path", "Exe", "Args", "Profile", "Sound",
    "Mouse", "CD required", "Notes"
};

#define FVX 20                  /* field value column */
#define FVW 52                  /* field value width  */

static void draw_form(GAME *g, int field)
{
    int i, y;
    char line[80];
    unsigned char a;

    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA DOS", A_TITLE);
    ui_puts(SCR_W - 12, 0, "Game Editor", UI_ATTR(C_WHITE, C_BLUE));

    ui_box(3, 2, 74, 18, A_FRAME);
    sprintf(line, " Editing [%s] ", g->id);
    ui_puts(5, 2, line, A_TITLE);

    for (i = 0; i < NFIELDS; i++) {
        y = 4 + i;
        if (i >= F_NOTES) y = 4 + i + 1;    /* small gap before notes */
        a = (i == field) ? A_ITEMSEL : A_ITEM;
        sprintf(line, " %-12s:", flabel[i]);
        ui_puts(5, y, line, (i == field) ? a : A_HINT);
        ui_fill(FVX, y, FVW, 1, ' ', a);
        switch (i) {
        case F_NAME:  ui_putlim(FVX, y, g->name, FVW, a); break;
        case F_PATH:  ui_putlim(FVX, y, g->path, FVW, a); break;
        case F_EXE:   ui_putlim(FVX, y, g->exe, FVW, a); break;
        case F_ARGS:  ui_putlim(FVX, y, g->args, FVW, a); break;
        case F_PROFILE: ui_putlim(FVX, y, g->profile, FVW, a); break;
        case F_SOUND:   ui_putlim(FVX, y, g->sound, FVW, a); break;
        case F_MOUSE: ui_puts(FVX, y, g->mouse ? "yes" : "no", a); break;
        case F_CD:    ui_puts(FVX, y, g->requires_cd ? "yes" : "no", a); break;
        case F_NOTES: ui_putlim(FVX, y, g->notes, FVW, a); break;
        }
    }

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    if (field == F_PROFILE || field == F_SOUND ||
        field == F_MOUSE || field == F_CD)
        ui_puts(2, SCR_H - 1,
            " Up/Down Field   Left/Right Change   Esc Back to list",
            A_STATUS);
    else
        ui_puts(2, SCR_H - 1,
            " Up/Down Field   Enter Edit text   Esc Back to list",
            A_STATUS);
}

/* Edit one text field via the inline editor. */
static void edit_text(char *buf, int maxlen, int field)
{
    char work[160];
    int y = 4 + field;
    if (field >= F_NOTES) y = 4 + field + 1;
    copystr(work, buf, (int)sizeof(work));
    if (ui_editline(work, maxlen, FVX, y, FVW)) {
        if (strcmp(work, buf) != 0) {
            copystr(buf, work, maxlen);
            dirty = 1;
        }
    }
}

static void edit_form(int idx)
{
    GAME *g = &games[idx];
    int field = 0, key, d;

    for (;;) {
        draw_form(g, field);
        key = ui_getkey();
        if (key == KEY_ESC) {
            /* Regenerate id if the name changed and id looks auto. */
            if (g->id[0] == '\0')
                gen_id(g, idx);
            return;
        } else if (key == KEY_UP) {
            field = (field > 0) ? field - 1 : NFIELDS - 1;
        } else if (key == KEY_DOWN) {
            field = (field < NFIELDS - 1) ? field + 1 : 0;
        } else if (key == KEY_LEFT || key == KEY_RIGHT) {
            d = (key == KEY_RIGHT) ? 1 : -1;
            if (field == F_PROFILE) {
                int p = enum_index(profiles, NPROF, g->profile);
                p = (p + NPROF + d) % NPROF;
                copystr(g->profile, profiles[p], sizeof(g->profile));
                dirty = 1;
            } else if (field == F_SOUND) {
                int p = enum_index(sounds, NSOUND, g->sound);
                p = (p + NSOUND + d) % NSOUND;
                copystr(g->sound, sounds[p], sizeof(g->sound));
                dirty = 1;
            } else if (field == F_MOUSE) {
                g->mouse = !g->mouse; dirty = 1;
            } else if (field == F_CD) {
                g->requires_cd = !g->requires_cd; dirty = 1;
            }
        } else if (key == KEY_ENTER) {
            switch (field) {
            case F_NAME:  edit_text(g->name, sizeof(g->name), field);  break;
            case F_PATH:  edit_text(g->path, sizeof(g->path), field);  break;
            case F_EXE:   edit_text(g->exe, sizeof(g->exe), field);    break;
            case F_ARGS:  edit_text(g->args, sizeof(g->args), field);  break;
            case F_NOTES: edit_text(g->notes, sizeof(g->notes), field);break;
            case F_MOUSE: g->mouse = !g->mouse; dirty = 1;             break;
            case F_CD:    g->requires_cd = !g->requires_cd; dirty = 1; break;
            default: break;
            }
        }
    }
}

/* --- list view ------------------------------------------------------ */

static void msg(const char *l1, const char *l2)
{
    int w = 54, h = 7, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    if (l1) ui_putlim(x + 3, y + 2, l1, w - 6, A_PANEL);
    if (l2) ui_putlim(x + 3, y + 4, l2, w - 6, A_PANEL);
    ui_getkey();
}

static int ask(const char *l1, const char *l2)
{
    int w = 54, h = 7, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2, k;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    if (l1) ui_putlim(x + 3, y + 2, l1, w - 6, A_PANEL);
    if (l2) ui_putlim(x + 3, y + 4, l2, w - 6, A_PANEL);
    for (;;) {
        k = ui_getkey();
        if (k == KEY_ENTER || k == 'y' || k == 'Y') return 1;
        if (k == KEY_ESC   || k == 'n' || k == 'N') return 0;
    }
}

#define LIST_Y 3
#define LVIS  18

static void draw_list(int sel, int top)
{
    int row, gi;
    char line[76];
    unsigned char attr;

    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA DOS", A_TITLE);
    ui_puts(SCR_W - 12, 0, "Game Editor", UI_ATTR(C_WHITE, C_BLUE));

    ui_box(2, 2, 76, 20, A_FRAME);
    ui_puts(4, 2, " Games ", A_TITLE);

    for (row = 0; row < LVIS; row++) {
        gi = top + row;
        ui_fill(3, LIST_Y + row, 74, 1, ' ', A_DESKTOP);
        if (gi >= ngames) continue;
        attr = (gi == sel) ? A_ITEMSEL : A_ITEM;
        if (gi == sel) ui_fill(3, LIST_Y + row, 74, 1, ' ', attr);
        sprintf(line, " %-30.30s %-6s %-6s %s", games[gi].name,
                games[gi].profile, games[gi].sound,
                games[gi].requires_cd ? "CD" : "  ");
        ui_puts(4, LIST_Y + row, line, attr);
    }

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(1, SCR_H - 1,
        " \x18\x19 Move  Enter Edit  A Add  Del Remove  F2 Save  Esc Quit",
        A_STATUS);
}

static void add_game(void)
{
    GAME *g;
    if (ngames >= MAX_GAMES) {
        msg("The game list is full.", "Remove a game before adding another.");
        return;
    }
    g = &games[ngames];
    memset(g, 0, sizeof(GAME));
    strcpy(g->name, "New Game");
    strcpy(g->exe, "GAME.EXE");
    strcpy(g->profile, "XMS");
    strcpy(g->sound, "NONE");
    gen_id(g, ngames);
    ngames++;
    dirty = 1;
    edit_form(ngames - 1);
}

int main(void)
{
    int sel = 0, top = 0, key;

    system("IF NOT EXIST C:\\CASTALIA\\BACKUP\\NUL "
           "MKDIR C:\\CASTALIA\\BACKUP >NUL");

    load_games();
    ui_init();

    for (;;) {
        if (sel < 0) sel = 0;
        if (sel >= ngames) sel = ngames ? ngames - 1 : 0;
        if (sel < top) top = sel;
        if (sel >= top + LVIS) top = sel - LVIS + 1;

        draw_list(sel, top);
        key = ui_getkey();

        if (key == KEY_ESC) {
            if (!dirty) break;
            if (ask("Save changes to GAMES.INI before leaving?",
                    "Enter/Y = save   N = discard")) {
                if (save_games() != 0)
                    msg("Save failed - is the disk writable?", "");
            }
            break;
        } else if (key == KEY_UP) {
            if (sel > 0) sel--;
        } else if (key == KEY_DOWN) {
            if (sel < ngames - 1) sel++;
        } else if (key == KEY_HOME) {
            sel = 0;
        } else if (key == KEY_END) {
            sel = ngames ? ngames - 1 : 0;
        } else if (key == KEY_ENTER) {
            if (ngames > 0) edit_form(sel);
        } else if (key == 'a' || key == 'A') {
            add_game();
            sel = ngames - 1;
        } else if (key == KEY_DEL) {
            if (ngames > 0 &&
                ask("Delete this game from the list?",
                    "Enter/Y = delete   N = keep")) {
                int i;
                for (i = sel; i < ngames - 1; i++)
                    games[i] = games[i + 1];
                ngames--;
                dirty = 1;
            }
        } else if (key == KEY_F2) {
            if (save_games() == 0)
                msg("Saved GAMES.INI.", "Previous version backed up.");
            else
                msg("Save failed - is the disk writable?", "");
        }
    }

    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
