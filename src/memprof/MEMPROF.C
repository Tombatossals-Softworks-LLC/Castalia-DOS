/* ===================================================================
 * MEMPROF.C  -  CASTALIA DOS Memory Profiles  (MEMPROF.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Describes the eight boot profiles (from PROFILES.INI) and shows the
 * machine's live memory picture, so the user can pick the right profile
 * for a game.  Switching profiles in real-mode DOS requires a reboot, so
 * this tool is informational: it explains how to switch rather than
 * re-laying-out drivers live.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os memprof.c ..\common\ini.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include "../common/INI.H"
#include "../common/UI.H"

#define MAX_PROF 12

typedef struct {
    char id[12];
    char name[32];
    int  conv_kb;
} PROF;

static PROF prof[MAX_PROF];
static int  nprof = 0;
static char cur_profile[12] = "";

static void copystr(char *dst, const char *src, int size)
{
    if (src == NULL) { dst[0] = '\0'; return; }
    strncpy(dst, src, (size_t)(size - 1));
    dst[size - 1] = '\0';
}

/* Load profile summaries from PROFILES.INI. */
static int load_profiles(void)
{
    static const char *cand[] = {
        "C:\\CASTALIA\\CFG\\PROFILES.INI",
        "PROFILES.INI",
        "config\\PROFILES.INI"
    };
    int i, n;
    const char *sec, *nm;

    for (i = 0; i < (int)(sizeof(cand) / sizeof(cand[0])); i++)
        if (ini_open(cand[i]) == INI_OK)
            break;
    if (i == (int)(sizeof(cand) / sizeof(cand[0])))
        return 0;

    n = ini_section_count();
    for (i = 0; i < n && nprof < MAX_PROF; i++) {
        sec = ini_section_name(i);
        if (sec == NULL)
            continue;
        nm = ini_get(sec, "name");
        if (nm == NULL)
            continue;
        copystr(prof[nprof].id, sec, sizeof(prof[0].id));
        copystr(prof[nprof].name, nm, sizeof(prof[0].name));
        prof[nprof].conv_kb = ini_get_int(sec, "conv_kb", 0);
        nprof++;
    }
    return 1;
}

/* Live conventional memory total (INT 12h), in KB. */
static unsigned conv_total_kb(void)
{
    union REGS r;
    int86(0x12, &r, &r);
    return r.x.ax;
}

/* Largest free conventional block (INT 21h AH=48h, BX=FFFF), in KB.
 * The allocation intentionally fails; DOS returns the largest available
 * size in BX (paragraphs). */
static unsigned largest_free_kb(void)
{
    union REGS r;
    r.h.ah = 0x48;
    r.x.bx = 0xFFFF;
    int86(0x21, &r, &r);
    return (unsigned)(((unsigned long)r.x.bx * 16UL) / 1024UL);
}

/* --- UI ------------------------------------------------------------- */

#define LX 3
#define LY 3
#define LW 30

static void draw_detail(int sel)
{
    int x = LX + LW + 3, y = LY + 1, w = 40;
    const char *sec = prof[sel].id;
    char line[72];

    ui_fill(LX + LW + 2, LY + 1, 42, 16, ' ', A_DESKTOP);

    ui_puts(x, y, prof[sel].name, UI_ATTR(C_YELLOW, C_BLUE));
    y += 2;
    sprintf(line, "Boot as menu item: %s", sec);
    ui_putlim(x, y++, line, w, A_ITEM);
    sprintf(line, "Target free conventional: ~%d KB", prof[sel].conv_kb);
    ui_putlim(x, y++, line, w, A_ITEM);
    y++;

    ui_putlim(x, y++, ini_get_def(sec, "summary", ""), w, A_ITEM);
    y++;
    /* The values come straight from PROFILES.INI and can be any length;
     * 20 fixed characters plus three 16-character fields fit line[72]. */
    sprintf(line, "HIMEM %.16s   EMS %.16s   UMB %.16s",
            ini_get_def(sec, "himem", "?"),
            ini_get_def(sec, "ems", "?"),
            ini_get_def(sec, "umb", "?"));
    ui_putlim(x, y++, line, w, A_HINT);
    y++;
    ui_puts(x, y++, "Good for:", UI_ATTR(C_WHITE, C_BLUE));
    ui_putlim(x, y++, ini_get_def(sec, "games", "-"), w, A_ITEM);
}

static void draw_screen(int sel)
{
    int i;
    unsigned char attr;
    char line[72];
    unsigned total, freek;

    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA DOS", A_TITLE);
    ui_puts(SCR_W - 18, 0, "Memory Profiles", UI_ATTR(C_WHITE, C_BLUE));

    /* Live memory line. */
    total = conv_total_kb();
    freek = largest_free_kb();
    sprintf(line, "Now: %s   Conventional %u KB total, %u KB free",
            cur_profile[0] ? cur_profile : "?", total, freek);
    ui_puts(3, 1, line, UI_ATTR(C_LGREEN, C_BLUE));

    /* Profile list. */
    ui_box(LX, LY, LW, nprof + 2, A_FRAME);
    ui_puts(LX + 2, LY, " Profiles ", A_TITLE);
    for (i = 0; i < nprof; i++) {
        int isnow = (strcmp(prof[i].id, cur_profile) == 0);
        attr = (i == sel) ? A_ITEMSEL : A_ITEM;
        ui_fill(LX + 1, LY + 1 + i, LW - 2, 1, ' ', attr);
        sprintf(line, "%c%-*.*s", isnow ? '*' : ' ',
                LW - 3, LW - 3, prof[i].name);
        ui_puts(LX + 1, LY + 1 + i, line, attr);
    }

    ui_box(LX + LW + 2, LY, 42, 18, A_FRAME);
    ui_puts(LX + LW + 4, LY, " Details ", A_TITLE);
    if (nprof > 0)
        draw_detail(sel);

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(2, SCR_H - 1,
        " Up/Down Select   * = current profile   Esc Quit   "
        "(reboot to switch)", A_STATUS);
}

int main(void)
{
    int sel = 0, key;
    const char *env;

    env = getenv("CASTPROFILE");
    if (env != NULL && env[0])
        copystr(cur_profile, env, sizeof(cur_profile));

    if (!load_profiles()) {
        printf("MEMPROF: PROFILES.INI not found.\n");
        printf("Expected at C:\\CASTALIA\\CFG\\PROFILES.INI\n");
        return 1;
    }

    /* Start on the current profile if we can find it. */
    if (cur_profile[0]) {
        int i;
        for (i = 0; i < nprof; i++)
            if (strcmp(prof[i].id, cur_profile) == 0) { sel = i; break; }
    }

    ui_init();
    for (;;) {
        draw_screen(sel);
        key = ui_getkey();
        if (key == KEY_ESC)
            break;
        else if (key == KEY_UP)
            sel = (sel > 0) ? sel - 1 : nprof - 1;
        else if (key == KEY_DOWN)
            sel = (sel < nprof - 1) ? sel + 1 : 0;
        else if (key == KEY_HOME)
            sel = 0;
        else if (key == KEY_END)
            sel = nprof - 1;
    }
    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
