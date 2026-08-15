/* ===================================================================
 * SPK.H  -  PC-speaker tones shared by the CASTALIA DOS suite
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * The motherboard beeper is the one audio output DOS can drive on every
 * machine - jumperless, driverless, present on a bare 386SX - so it is
 * what the Castalia tools and minigames use.  8253 timer channel 2,
 * gated at port 61h, exactly as SETSOUND's test tone did before this
 * module existed (see docs/SOUND.md 9.x, profile 1).
 *
 * Two ways to make a noise:
 *
 *   spk_tone()  blocks for the whole note.  Fine for a menu chime or a
 *               confirmation beep, wrong inside a game loop.
 *   spk_note()  starts a note and returns AT ONCE; the caller then calls
 *               spk_poll() once per loop turn and the note is silenced
 *               when its deadline passes.  Nothing ever stalls, which is
 *               what a 386SX needs while a boulder is in the air.
 *
 * Silence is a first-class state: spk_init() honours the CASTSOUND
 * environment variable (OFF/NO/0 mute the suite), games offer their own
 * S key, and every entry point is a no-op while muted.  A program that
 * makes noise must leave the speaker OFF when it exits - call spk_off().
 *
 * Port I/O is compiler-guarded (Open Watcom inp/outp, Turbo C
 * inportb/outportb, no-ops for the host syntax gate), the same shape
 * CPUDET.C uses.  C89, no allocation, no interrupts hooked, no TSR.
 * =================================================================== */

#ifndef CASTALIA_SPK_H
#define CASTALIA_SPK_H

/* Read CASTSOUND from the environment and set the mute state from it.
 * Safe to skip; the default is "sound on". */
void spk_init(void);

/* Global mute.  Games toggle this from their own S key. */
void spk_mute(int on);
int  spk_muted(void);

/* Blocking: sound 'freq' Hz for 'ticks' BIOS ticks (18.2/s), then stop. */
void spk_tone(unsigned freq, int ticks);

/* Non-blocking: start 'freq' Hz and return.  spk_poll() silences it once
 * 'ticks' BIOS ticks have passed.  A later spk_note() overrides an
 * un-expired one, so arpeggios are just repeated calls. */
void spk_note(unsigned freq, int ticks);

/* Call once per game-loop turn.  Cheap; does nothing unless a note is
 * pending and due.  Handles the BIOS tick's midnight wrap. */
void spk_poll(void);

/* Silence the speaker now and forget any pending note.  Always call this
 * before leaving the program. */
void spk_off(void);

/* A few ready-made voices, so every tool sounds like the same product.
 * Each is non-blocking (spk_note-based) unless noted. */
void spk_blip(void);        /* cursor move / tile placed        */
void spk_ok(void);          /* a good thing happened            */
void spk_bad(void);         /* rejected input, illegal move     */
void spk_boom(void);        /* an impact                        */
void spk_fanfare(void);     /* victory - BLOCKING, end of round */

#endif /* CASTALIA_SPK_H */
