/* ===================================================================
 * SPK.C  -  PC-speaker tones shared by the CASTALIA DOS suite
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * See SPK.H for the contract.  The hardware is the 8253/8254 timer,
 * channel 2, whose output is ANDed with bit 1 of port 61h and wired to
 * the speaker cone:
 *
 *   43h  <- B6h    channel 2, access lo/hi, mode 3 (square wave), binary
 *   42h  <- divisor low byte, then high byte   (1193182 / frequency)
 *   61h  <- bits 0+1 set: gate the timer through to the cone
 *   61h  <- bits 0+1 cleared: silence, restoring the other bits
 *
 * The other bits of 61h belong to the rest of the machine (parity, RAM
 * refresh), so every write reads the port first and only touches bits
 * 0 and 1 - clobbering the rest can wedge a real 386SX.
 * =================================================================== */

#include <stdlib.h>
#include <string.h>
#include "UI.H"
#include "SPK.H"

/* ---- port I/O, per compiler ---------------------------------------
 * Real hardware needs in/out instructions; the host syntax gate has no
 * ports, so it compiles these to no-ops and every routine below turns
 * into harmless arithmetic.  Same shape as CPUDET.C. */
#if defined(__TURBOC__)
#  include <dos.h>
#  define OUTP(p, v) outportb((p), (unsigned char)(v))
#  define INP(p)     inportb((p))
#elif defined(__WATCOMC__)
#  include <conio.h>
#  define OUTP(p, v) outp((p), (unsigned char)(v))
#  define INP(p)     inp((p))
#else
#  define OUTP(p, v) ((void)(p), (void)(v))
#  define INP(p)     ((void)(p), (unsigned char)0)
#endif

#define PIT_HZ      1193182UL       /* the 8253 input clock            */
#define PORT_CTRL   0x43
#define PORT_CH2    0x42
#define PORT_GATE   0x61

static int           muted;         /* 0 = audible                     */
static int           sounding;      /* the cone is currently driven    */
static int           pending;       /* a spk_note() is waiting to end  */
static unsigned long deadline;      /* ...at this BIOS tick            */

/* Drive the cone at 'freq' Hz.  Frequencies outside the divisor's range
 * are refused rather than programming a nonsense divisor; the caller is
 * told so it does not schedule a note that never started. */
static int gate_on(unsigned freq)
{
    unsigned div;
    unsigned char gate;

    if (freq < 20U || freq > 20000U)
        return 0;
    div = (unsigned)(PIT_HZ / (unsigned long)freq);
    OUTP(PORT_CTRL, 0xB6);                  /* ch 2, mode 3, square    */
    OUTP(PORT_CH2, (unsigned char)(div & 0xFF));
    OUTP(PORT_CH2, (unsigned char)(div >> 8));
    gate = (unsigned char)INP(PORT_GATE);
    OUTP(PORT_GATE, (unsigned char)(gate | 0x03));
    sounding = 1;
    return 1;
}

static void gate_off(void)
{
    unsigned char gate = (unsigned char)INP(PORT_GATE);
    OUTP(PORT_GATE, (unsigned char)(gate & 0xFC));   /* keep bits 2..7 */
    sounding = 0;
}

/* ---- state -------------------------------------------------------- */

void spk_init(void)
{
    const char *e = getenv("CASTSOUND");
    muted = 0;
    if (e != NULL &&
        (e[0] == '0' ||
         e[0] == 'n' || e[0] == 'N' ||           /* NO / no            */
         ((e[0] == 'o' || e[0] == 'O') &&
          (e[1] == 'f' || e[1] == 'F'))))        /* OFF / off          */
        muted = 1;
}

void spk_mute(int on)
{
    muted = on ? 1 : 0;
    if (muted)
        spk_off();
}

int spk_muted(void)
{
    return muted;
}

void spk_off(void)
{
    pending = 0;
    if (sounding)
        gate_off();
}

/* ---- making a noise ------------------------------------------------ */

void spk_tone(unsigned freq, int ticks)
{
    unsigned long until;

    if (muted || ticks <= 0)
        return;
    if (!gate_on(freq))         /* refused: never wait on silence */
        return;
    until = ui_ticks() + (unsigned long)ticks;
    for (;;) {
        unsigned long now = ui_ticks();
        if (now >= until)
            break;
        if (now + 100UL < until)            /* the tick wrapped        */
            break;
        ui_idle();
    }
    gate_off();
}

void spk_note(unsigned freq, int ticks)
{
    if (muted || ticks <= 0)
        return;
    if (!gate_on(freq))         /* refused: nothing to silence later */
        return;
    deadline = ui_ticks() + (unsigned long)ticks;
    pending = 1;
}

void spk_poll(void)
{
    unsigned long now;

    if (!pending)
        return;
    now = ui_ticks();
    if (now >= deadline || now + 100UL < deadline) {   /* due, or wrapped */
        pending = 0;
        gate_off();
    }
}

/* ---- the house voices ---------------------------------------------- */

void spk_blip(void)  { spk_note(1760U, 1); }   /* A6, one tick         */
void spk_ok(void)    { spk_note(1319U, 2); }   /* E6                   */
void spk_bad(void)   { spk_note(160U,  2); }   /* a low thud           */
void spk_boom(void)  { spk_note(70U,   4); }   /* the lowest we allow  */

void spk_fanfare(void)
{
    /* Blocking on purpose: it plays at the end of a round, when the game
     * has already stopped and a panel is up. */
    spk_tone(880U, 2);      /* A5  */
    spk_tone(1109U, 2);     /* C#6 */
    spk_tone(1319U, 2);     /* E6  */
    spk_tone(1760U, 4);     /* A6  */
}
