/* ===================================================================
 * SNAKE.C  -  CASTALIA SNAKE  (SNAKE.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * The first Castalia minigame: classic snake in 80x25 text mode.
 * Runs happily on a 386SX: incremental drawing (only the head and tail
 * cells change per step), BIOS-tick pacing, keyboard polling through
 * the BIOS buffer.  No TSR, no timers hooked, exits clean.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os snake.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/UI.H"
#include "../common/SPK.H"

/* Play field: interior of a box from (0,1) to (79,23). */
#define FX0 1
#define FY0 2
#define FX1 78
#define FY1 22

#define MAXBODY 1700

static int  sx[MAXBODY], sy[MAXBODY];
static int  head, tail, count;
static unsigned char occ[25][80];
static int  dx, dy, pdx, pdy;
static int  foods, speed, grow;
static int  fx, fy;

static void put_status(void)
{
    char buf[96];
    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    sprintf(buf, " Length %d  Apples %d  Speed %d   "
            "Arrows - P pause - S sound %s - Esc quit",
            count, foods, 4 - speed, spk_muted() ? "off" : "on");
    ui_puts(1, SCR_H - 1, buf, A_STATUS);
}

/* Wait for a key without ever leaving a note ringing: the beeper's
 * deadline only expires inside spk_poll(), so a blocking ui_getkey()
 * would hold the last note until the player pressed something. */
static int wait_key(void)
{
    while (!ui_keywaiting()) {
        spk_poll();
        ui_idle();
    }
    spk_poll();
    return ui_getkey();
}

static void place_food(void)
{
    do {
        fx = FX0 + rand() % (FX1 - FX0 + 1);
        fy = FY0 + rand() % (FY1 - FY0 + 1);
    } while (occ[fy][fx]);
    ui_putc(fx, fy, (char)0x04, UI_ATTR(C_YELLOW, C_BLUE));
}

static void new_game(void)
{
    int i;
    memset(occ, 0, sizeof(occ));
    head = tail = 0;
    count = 0;
    for (i = 0; i < 4; i++) {           /* start with 4 segments */
        sx[head] = 20 + i;
        sy[head] = 12;
        occ[12][20 + i] = 1;
        head = (head + 1) % MAXBODY;
        count++;
    }
    dx = 1; dy = 0; pdx = 1; pdy = 0;
    foods = 0;
    speed = 3;
    grow = 0;

    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA SNAKE", A_TITLE);
    ui_box(0, 1, SCR_W, 23, A_FRAME);
    for (i = 0; i < 4; i++)
        ui_putc(20 + i, 12, (char)0xDB, UI_ATTR(C_LGREEN, C_BLUE));
    put_status();
    place_food();
}

static void poll_keys(int *quit, int *pause)
{
    int k;
    while (ui_keywaiting()) {
        k = ui_getkey();
        if (k == KEY_ESC) { *quit = 1; return; }
        else if (k == 'p' || k == 'P') { *pause = 1; }
        else if (k == 's' || k == 'S') { spk_mute(!spk_muted());
                                         put_status(); }
        else if (k == KEY_UP    && dy != 1)  { pdx = 0;  pdy = -1; }
        else if (k == KEY_DOWN  && dy != -1) { pdx = 0;  pdy = 1;  }
        else if (k == KEY_LEFT  && dx != 1)  { pdx = -1; pdy = 0;  }
        else if (k == KEY_RIGHT && dx != -1) { pdx = 1;  pdy = 0;  }
    }
}

/* Returns 1 to play again, 0 to exit. */
static int game_over(void)
{
    int w = 40, h = 7, x = (SCR_W - w) / 2, y = 9, k, i;
    int hx = sx[(head + MAXBODY - 1) % MAXBODY];
    int hy = sy[(head + MAXBODY - 1) % MAXBODY];
    unsigned long t;

    spk_boom();                         /* the snake runs out of luck */
    for (i = 0; i < 6; i++) {           /* crash flash */
        ui_putc(hx, hy, (char)0xDB,
                (i & 1) ? UI_ATTR(C_LGREEN, C_BLUE)
                        : UI_ATTR(C_WHITE, C_RED));
        t = ui_ticks() + 2;
        while (ui_ticks() < t) { spk_poll(); }
    }
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " The serpent falls ", A_PANELHDR);
    {
        char buf[40];
        sprintf(buf, "Final length: %d   Apples: %d", count, foods);
        ui_puts(x + 3, y + 2, buf, A_PANEL);
    }
    ui_puts(x + 3, y + 4, "R play again      Esc leave", A_PANEL);
    /* The round is over and the panel is up, so blocking is fair here. */
    spk_tone(220, 3);                   /* the serpent falls... */
    spk_tone(165, 3);
    spk_tone(110, 6);
    for (;;) {
        k = wait_key();
        if (k == 'r' || k == 'R') return 1;
        if (k == KEY_ESC) return 0;
    }
}

int main(void)
{
    int quit = 0, pause = 0;
    unsigned long next;

    srand((unsigned)ui_ticks());
    ui_init();
    spk_init();

restart:
    new_game();
    next = ui_ticks() + (unsigned long)speed;

    for (;;) {
        /* Pace one step per 'speed' ticks, polling input meanwhile. */
        for (;;) {
            spk_poll();
            poll_keys(&quit, &pause);
            if (quit || pause)
                break;
            if (ui_ticks() >= next)
                break;
            if (ui_ticks() + 20UL < next)   /* midnight wrap guard */
                next = ui_ticks();
        }
        if (quit)
            break;
        if (pause) {
            ui_puts(36, 12, " PAUSED ", A_ITEMSEL);
            wait_key();
            ui_putc(36, 12, ' ', A_DESKTOP);
            ui_fill(36, 12, 8, 1, ' ', A_DESKTOP);
            pause = 0;
            next = ui_ticks() + (unsigned long)speed;
            continue;
        }
        next += (unsigned long)speed;

        dx = pdx; dy = pdy;
        {
            int hx = sx[(head + MAXBODY - 1) % MAXBODY] + dx;
            int hy = sy[(head + MAXBODY - 1) % MAXBODY] + dy;

            if (hx < FX0 || hx > FX1 || hy < FY0 || hy > FY1 ||
                occ[hy][hx]) {
                if (game_over())
                    goto restart;
                break;
            }
            /* Dim the old head into a body segment. */
            ui_putc(sx[(head + MAXBODY - 1) % MAXBODY],
                    sy[(head + MAXBODY - 1) % MAXBODY],
                    (char)0xDB, UI_ATTR(C_GREEN, C_BLUE));
            /* New head. */
            sx[head] = hx; sy[head] = hy;
            occ[hy][hx] = 1;
            head = (head + 1) % MAXBODY;
            count++;
            ui_putc(hx, hy, (char)0xDB, UI_ATTR(C_LGREEN, C_BLUE));

            if (hx == fx && hy == fy) {
                foods++;
                grow += 3;
                if (foods % 5 == 0 && speed > 1)
                    speed--;
                /* One blip per apple, climbing a step at a time so the
                 * beeper speeds up along with the snake. */
                spk_note((unsigned)(784 + (foods % 16) * 55), 2);
                place_food();
                put_status();
            }
            if (grow > 0) {
                grow--;
            } else {
                occ[sy[tail]][sx[tail]] = 0;
                ui_putc(sx[tail], sy[tail], ' ', A_DESKTOP);
                tail = (tail + 1) % MAXBODY;
                count--;
            }
        }
    }

    spk_off();
    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
