/* ===================================================================
 * SIEGE.C  -  CASTALIA ASEDIO: catapult duel  (SIEGE.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * "Asedio" is Spanish for a siege: two Mediterranean fortresses trade
 * catapult fire across a jagged skyline.  Set the angle and the power,
 * read the wind, and drop a boulder on the enemy keep before its
 * gunners bracket yours.  A turn-based artillery duel in Castalia
 * colours, first to three hits takes the field.
 *
 * 386SX friendly: the boulder animates cell by cell over the sky (erase
 * the old cell, draw the new), BIOS-tick pacing, keyboard through the
 * BIOS buffer, no TSRs.  All flight physics run in 32-bit long with a
 * self-made integer sine table -- no floating point, no <math.h>.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os siege.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/UI.H"
#include "../common/SPK.H"

/* --- Integer trig: sin(deg) * 1000, degrees 0..90 ------------------
 * cos(a) = sintab[90 - a].  Values fit comfortably in an int.        */
static const int sintab[91] = {
       0,  17,  35,  52,  70,  87, 105, 122, 139, 156,
     174, 191, 208, 225, 242, 259, 276, 292, 309, 326,
     342, 358, 375, 391, 407, 423, 438, 454, 469, 485,
     500, 515, 530, 545, 559, 574, 588, 602, 616, 629,
     643, 656, 669, 682, 695, 707, 719, 731, 743, 755,
     766, 777, 788, 799, 809, 819, 829, 839, 848, 857,
     866, 875, 883, 891, 899, 906, 914, 921, 927, 934,
     940, 946, 951, 956, 961, 966, 970, 974, 978, 982,
     985, 988, 990, 993, 995, 996, 998, 999, 999,1000,
    1000
};

/* --- Physics scaling (kept small so nothing overflows a 16-bit int
 * on the real Watcom build; the heavy products are done in long).
 *   FP     : sub-cell fixed point, 256 units == one screen cell
 *   PDIV   : divides power*trig into a launch velocity
 *   GRAV   : downward acceleration added to vy every step
 *   WDRIFT : sideways cells-per-step drift contributed by the wind  */
#define FP      256L
#define PDIV    260L
#define GRAV      7L
#define WDRIFT   10L

#define A_SKY   A_DESKTOP

#define PLAYER  0
#define CPU     1

#define KH      4               /* keep tower height in rows          */
#define KEEPG  20               /* ground row the keeps stand on      */

/* --- Game state -------------------------------------------------------- */
static int ground[80];          /* topmost terrain row for each column  */
static int wind;                /* signed, -5..+5, positive blows right */
static int p_score, c_score;

static int angle, power;        /* the current shooter's dial values    */
static int pl_angle, pl_power;  /* the player's persistent dials        */

/* Keep footprints (columns x0..x1, tower rows top..g-1, base row g). */
static int pk_x0, pk_x1, pk_top, pk_g;
static int ck_x0, ck_x1, ck_top, ck_g;
static int pox, poy;            /* player launch origin (for the guide) */

/* CPU bracketing gunner. */
static int cpu_pmin, cpu_pmax, cpu_power, cpu_angle;

/* Last impact cell (set by fire, read by the explosion). */
static int imp_x, imp_y;

/* --- Timing ------------------------------------------------------------ */
static void wait_ticks(int n)
{
    unsigned long until = ui_ticks() + (unsigned long)n;
    while (ui_ticks() < until) {
        spk_poll();
        ui_idle();
        if (ui_ticks() + 100UL < until)
            break;              /* midnight-wrap guard */
    }
    spk_poll();
}

/* Wait for a key while the speaker keeps its own time: a pending note
 * only expires inside spk_poll(), so a bare ui_getkey() would hold the
 * last one until the gunner pressed something. */
static int wait_key(void)
{
    while (!ui_keywaiting()) {
        spk_poll();
        ui_idle();
    }
    spk_poll();
    return ui_getkey();
}

/* --- Drawing ----------------------------------------------------------- */
static void draw_keep(int x0, int x1, int top, int g, unsigned char fg)
{
    int x, y;
    unsigned char a = UI_ATTR(fg, C_BLACK);
    for (y = top; y < g; y++)
        for (x = x0; x <= x1; x++)
            ui_putc(x, y, (char)0xDB, a);
    /* Battlemented crest: carve merlons out of the top row. */
    for (x = x0; x <= x1; x++)
        if (((x - x0) & 1) == 1)
            ui_putc(x, top, ' ', A_SKY);
    /* Banner on a corner merlon. */
    ui_putc(x0, top - 1, (char)0x1E, UI_ATTR(fg, C_BLUE));
}

static void draw_status(int who)
{
    char buf[100];
    char wc;
    int wm = (wind < 0) ? -wind : wind;
    wc = (wind < 0) ? '<' : (wind > 0) ? '>' : '=';
    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    if (who == PLAYER)
        sprintf(buf, " Tu %d-CPU %d  Ang %2d  Pot %2d  Viento %c%d  "
                "Arr/Ab  Izq/Der  Enter fuego",
                p_score, c_score, angle, power, wc, wm);
    else
        sprintf(buf, " Tu %d-CPU %d  Ang %2d  Pot %2d  Viento %c%d  "
                "La CPU apunta y dispara...",
                p_score, c_score, angle, power, wc, wm);
    /* The dials take the left 68 columns; the beeper's state keeps the
     * right-hand corner to itself so neither can push the other off. */
    ui_putlim(1, SCR_H - 1, buf, 68, A_STATUS);
    ui_puts(70, SCR_H - 1, spk_muted() ? "S mudo  " : "S sonido", A_STATUS);
}

static void draw_scene(void)
{
    int x, y;
    ui_cls(A_SKY);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA ASEDIO", A_TITLE);
    ui_puts(20, 0, "Duelo de catapultas al alba", UI_ATTR(C_WHITE, C_BLUE));

    for (x = 0; x < 80; x++)
        for (y = ground[x]; y < 24; y++)
            ui_putc(x, y, (char)0xDB,
                    (y == ground[x]) ? UI_ATTR(C_LGREEN, C_BLACK)
                                     : UI_ATTR(C_BROWN,  C_BLACK));

    draw_keep(pk_x0, pk_x1, pk_top, pk_g, C_YELLOW);
    draw_keep(ck_x0, ck_x1, ck_top, ck_g, C_LRED);
    ui_puts(pk_x0 - 1, pk_top - 2, "TU", A_TITLE);
    ui_puts(ck_x1 - 2, ck_top - 2, "CPU", UI_ATTR(C_LRED, C_BLUE));

    draw_status(PLAYER);
}

/* Straight-line aim guide from the player keep; recomputed from angle
 * so the same call erases exactly what it drew. */
static void draw_aim(int erase)
{
    int i, gx, gy;
    int cosv = sintab[90 - angle];
    int sinv = sintab[angle];
    for (i = 1; i <= 4; i++) {
        gx = pox + (int)(((long)i * cosv * 3) / 1000L);
        gy = poy - (int)(((long)i * sinv * 3) / 1000L);
        if (gx < 0 || gx > 79 || gy < 0 || gy > 23)
            continue;
        if (gy >= ground[gx])
            continue;
        ui_putc(gx, gy, erase ? ' ' : (char)0xFA,
                erase ? A_SKY : UI_ATTR(C_YELLOW, C_BLUE));
    }
}

static void explode(int x, int y)
{
    static const int ddx[5] = { 0, -1, 1, 0, 0 };
    static const int ddy[5] = { 0, 0, 0, -1, 1 };
    int f, i, ex, ey;
    for (f = 0; f < 6; f++) {
        unsigned char a = (f & 1) ? UI_ATTR(C_YELLOW, C_RED)
                                  : UI_ATTR(C_WHITE,  C_RED);
        if ((f & 3) == 0)
            spk_boom();                 /* two rumbles over six frames */
        for (i = 0; i < 5; i++) {
            ex = x + ddx[i];
            ey = y + ddy[i];
            if (ex >= 0 && ex < 80 && ey >= 0 && ey < 24)
                ui_putc(ex, ey, (char)0xB2, a);
        }
        wait_ticks(2);
    }
}

/* --- Terrain / round setup --------------------------------------------- */
static void new_round(void)
{
    int x, h;
    h = 18 + rand() % 3;
    for (x = 0; x < 80; x++) {
        h += (rand() % 3) - 1;          /* gentle random hills */
        if (h < 16) h = 16;
        if (h > 22) h = 22;
        ground[x] = h;
    }
    for (x = 6; x <= 13; x++) ground[x] = KEEPG;    /* flatten under keeps */
    for (x = 66; x <= 73; x++) ground[x] = KEEPG;

    pk_x0 = 8;  pk_x1 = 11; pk_g = KEEPG; pk_top = KEEPG - KH;
    ck_x0 = 68; ck_x1 = 71; ck_g = KEEPG; ck_top = KEEPG - KH;
    pox = 10; poy = pk_top - 1;

    wind = (rand() % 11) - 5;

    cpu_pmin = 30;
    cpu_pmax = 99;
    cpu_power = 62;
    cpu_angle = 48 + rand() % 7;

    draw_scene();
}

/* --- Ballistics -------------------------------------------------------- *
 * Steps a boulder from the shooter's keep.  Returns 2 on an enemy-keep
 * hit, 1 on any miss (own keep / terrain / off the sides).  On return
 * *landx holds the final column and imp_x/imp_y the impact cell.        */
static int fire(int shooter, int *landx)
{
    long fx, fy, vx, vy, wd;
    int ox, oy, dir;
    int ex0, ex1, etop, eg;             /* enemy keep box */
    int ox0, ox1, otop, og;             /* own keep box   */
    int cx, cy, prevx, prevy, steps, leftown, out;

    if (shooter == PLAYER) {
        ox = pox; oy = poy; dir = 1;
        ex0 = ck_x0; ex1 = ck_x1; etop = ck_top; eg = ck_g;
        ox0 = pk_x0; ox1 = pk_x1; otop = pk_top; og = pk_g;
    } else {
        ox = 69; oy = ck_top - 1; dir = -1;
        ex0 = pk_x0; ex1 = pk_x1; etop = pk_top; eg = pk_g;
        ox0 = ck_x0; ox1 = ck_x1; otop = ck_top; og = ck_g;
    }

    fx = (long)ox * FP;
    fy = (long)oy * FP;
    vx = (long)power * sintab[90 - angle] / PDIV;
    if (dir < 0)
        vx = -vx;
    vy = -((long)power * sintab[angle] / PDIV);
    wd = (long)wind * WDRIFT;

    cx = ox; cy = oy;
    prevx = -1; prevy = -1;
    steps = 0; leftown = 0; out = 1;

    /* The arm lets go: a harder shot cracks higher. */
    spk_note((unsigned)(200 + power * 6), 2);

    for (;;) {
        vy += GRAV;
        fx += vx + wd;
        fy += vy;
        cx = (int)(fx / FP);
        cy = (int)(fy / FP);
        steps++;

        if (cx < 0 || cx > 79) { out = 1; break; }
        if (!leftown && (cx < ox0 || cx > ox1))
            leftown = 1;
        if (cy >= 0) {
            if (cx >= ex0 && cx <= ex1 && cy >= etop && cy <= eg) {
                out = 2; break;
            }
            if (leftown && cx >= ox0 && cx <= ox1 && cy >= otop && cy <= og) {
                out = 1; break;
            }
            if (cy >= 23 || ground[cx] <= cy) { out = 1; break; }
        }

        /* Sky cell (or above the top edge): animate the boulder. */
        if (prevx >= 0)
            ui_putc(prevx, prevy, ' ', A_SKY);
        if (cy >= 0) {
            ui_putc(cx, cy, (char)0x07, UI_ATTR(C_WHITE, C_BLUE));
            prevx = cx; prevy = cy;
        } else {
            prevx = -1; prevy = -1;
        }
        if (steps & 1)
            wait_ticks(1);
        if (steps > 1500) { out = 1; break; }
    }

    if (prevx >= 0)
        ui_putc(prevx, prevy, ' ', A_SKY);
    imp_x = cx;
    imp_y = cy;
    if (out == 1)
        spk_note(147, 2);               /* the dull thud of a miss */
    if (landx)
        *landx = (cx < 0) ? 0 : (cx > 79 ? 79 : cx);
    return out;
}

/* --- Turns ------------------------------------------------------------- */
/* Returns 2 (hit), 1 (miss) or -1 (player asked to quit). */
static int player_turn(void)
{
    int k, lx;
    angle = pl_angle;
    power = pl_power;
    draw_status(PLAYER);
    draw_aim(0);
    for (;;) {
        if (!ui_keywaiting()) { spk_poll(); ui_idle(); continue; }
        spk_poll();
        k = ui_getkey();
        if (k == KEY_ESC) { draw_aim(1); return -1; }
        else if (k == 's' || k == 'S') {
            spk_mute(!spk_muted());
            draw_status(PLAYER);
        } else if (k == KEY_UP) {
            if (angle < 89) { draw_aim(1); angle++; draw_aim(0);
                              draw_status(PLAYER); }
        } else if (k == KEY_DOWN) {
            if (angle > 1)  { draw_aim(1); angle--; draw_aim(0);
                              draw_status(PLAYER); }
        } else if (k == KEY_LEFT) {
            if (power > 10) { power--; draw_status(PLAYER); }
        } else if (k == KEY_RIGHT) {
            if (power < 99) { power++; draw_status(PLAYER); }
        } else if (k == KEY_ENTER || k == KEY_SPACE) {
            break;
        }
    }
    pl_angle = angle;
    pl_power = power;
    draw_aim(1);
    return fire(PLAYER, &lx);
}

/* Returns 2 (hit) or 1 (miss). */
static int cpu_turn(void)
{
    int out, lx;
    power = cpu_power;
    angle = cpu_angle;
    draw_status(CPU);
    wait_ticks(9);                      /* let the player read the dials */
    out = fire(CPU, &lx);
    if (out != 2) {
        if (lx > 10)                    /* fell short (right of the keep) */
            cpu_pmin = cpu_power;       /* -> next shot needs more power  */
        else                            /* overshot to the left           */
            cpu_pmax = cpu_power;       /* -> next shot needs less power   */
        cpu_power = (cpu_pmin + cpu_pmax) / 2 + (rand() % 5 - 2);
        if (cpu_power < 30) cpu_power = 30;
        if (cpu_power > 99) cpu_power = 99;
    }
    return out;
}

/* Returns 1 to replay, 0 to leave. */
static int match_over(int player_won)
{
    int w = 46, h = 8, x = (SCR_W - w) / 2, y = 8, k;
    char buf[48];
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, player_won ? " Castalia rompe el asedio "
                                  : " La fortaleza ha caido ", A_PANELHDR);
    sprintf(buf, "Marcador final   Tu %d  -  CPU %d", p_score, c_score);
    ui_puts(x + 3, y + 2, buf, A_PANEL);
    ui_puts(x + 3, y + 3, player_won
            ? "Tus catapultas guardan la muralla."
            : "El enemigo planta su estandarte.", A_PANEL);
    ui_puts(x + 3, y + 5, "R  jugar de nuevo        Esc  salir", A_PANEL);
    /* The match is decided and a panel is up, so blocking is fair here. */
    if (player_won) {
        spk_fanfare();
    } else {
        spk_tone(147, 4);
        spk_tone(98, 10);
    }
    for (;;) {
        k = wait_key();
        if (k == 'r' || k == 'R') return 1;
        if (k == KEY_ESC) return 0;
    }
}

int main(void)
{
    int out, quit = 0;

    srand((unsigned)ui_ticks());
    ui_init();
    spk_init();

replay:
    p_score = 0;
    c_score = 0;
    pl_angle = 45;
    pl_power = 50;
    new_round();

    while (!quit) {
        out = player_turn();
        if (out < 0) { quit = 1; break; }
        if (out == 2) {
            explode(imp_x, imp_y);
            p_score++;
            if (p_score >= 3) {
                if (match_over(1)) goto replay;
                break;
            }
            new_round();
            continue;
        }

        out = cpu_turn();
        if (out == 2) {
            explode(imp_x, imp_y);
            c_score++;
            if (c_score >= 3) {
                if (match_over(0)) goto replay;
                break;
            }
            new_round();
            continue;
        }
        /* Both missed: the duel goes on. */
    }

    spk_off();
    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
