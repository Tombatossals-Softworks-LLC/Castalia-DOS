/* ===================================================================
 * test_spk.c  -  host unit tests for the shared PC-speaker module
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * SPK's timing is the part that can quietly go wrong: a note that is
 * never silenced leaves a real 386SX droning, and the BIOS tick resets
 * at midnight, so "is it due yet?" is not just now >= deadline.
 *
 * The module is #included (not linked) so the tests can see its static
 * state - pending/sounding are exactly what we need to assert on - and
 * ui_ticks() is replaced by a clock this file drives by hand, which is
 * how a note's whole life can be examined without waiting for real time.
 * On this host the port writes compile to no-ops (SPK.C's #else branch),
 * so nothing here touches hardware.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "UI.H"

/* --- the fake clock, in place of the BIOS tick ---------------------- */
static unsigned long fake_now = 1000UL;
unsigned long ui_ticks(void) { return fake_now; }
void ui_idle(void) { }

#include "SPK.C"

/* putenv() is POSIX, not C89, so -std=c89 hides its declaration; declare
 * it here and hand it writable buffers as the interface requires. */
extern int putenv(char *);

static void set_castsound(const char *value)
{
    static char buf[32];
    strcpy(buf, "CASTSOUND=");
    strcat(buf, value);
    putenv(buf);
}

static int checks = 0, failures = 0;

static void ck(const char *what, int cond)
{
    checks++;
    if (cond) {
        printf("  [  OK  ] %s\n", what);
    } else {
        printf("  [ FAIL ] %s\n", what);
        failures++;
    }
}

int main(void)
{
    printf("== SPK: the speaker is silent unless asked ==\n");
    fake_now = 1000UL;
    spk_mute(0);
    spk_off();
    ck("nothing pending at rest", !pending);
    ck("cone quiet at rest", !sounding);

    printf("\n== SPK: a note starts at once and returns ==\n");
    spk_note(1000U, 5);
    ck("note is pending", pending == 1);
    ck("cone is driven", sounding == 1);
    ck("deadline is now + ticks", deadline == 1005UL);

    printf("\n== SPK: spk_poll() holds the note, then releases it ==\n");
    fake_now = 1004UL;
    spk_poll();
    ck("still sounding one tick early", sounding == 1 && pending == 1);
    fake_now = 1005UL;
    spk_poll();
    ck("silenced exactly on the deadline", sounding == 0);
    ck("nothing left pending", !pending);
    spk_poll();
    ck("a second poll is harmless", !pending && !sounding);

    printf("\n== SPK: the midnight wrap cannot strand a note ==\n");
    fake_now = 1500000UL;               /* just before the BIOS resets  */
    spk_note(800U, 10);
    ck("note pending before midnight", pending == 1 && sounding == 1);
    fake_now = 3UL;                     /* the tick counter wrapped     */
    spk_poll();
    ck("wrap silences instead of hanging", sounding == 0 && !pending);

    printf("\n== SPK: a new note overrides an unexpired one ==\n");
    fake_now = 100UL;
    spk_note(400U, 50);
    spk_note(900U, 2);
    ck("deadline follows the newest note", deadline == 102UL);
    fake_now = 102UL;
    spk_poll();
    ck("the newest note ends on time", !sounding);

    printf("\n== SPK: muting silences and stays silent ==\n");
    fake_now = 200UL;
    spk_note(1000U, 5);
    spk_mute(1);
    ck("muting stops a sounding note", sounding == 0 && !pending);
    ck("spk_muted() reports the state", spk_muted() == 1);
    spk_note(1000U, 5);
    ck("spk_note is a no-op while muted", !pending && !sounding);
    spk_blip();
    spk_ok();
    spk_bad();
    spk_boom();
    ck("the house voices stay silent too", !pending && !sounding);
    spk_tone(1000U, 3);
    ck("spk_tone is a no-op while muted", !sounding);
    spk_mute(0);
    ck("unmuting restores audibility", spk_muted() == 0);

    printf("\n== SPK: CASTSOUND decides before any program runs ==\n");
    set_castsound("OFF");
    spk_init();
    ck("CASTSOUND=OFF mutes", spk_muted() == 1);
    set_castsound("no");
    spk_init();
    ck("CASTSOUND=no mutes", spk_muted() == 1);
    set_castsound("0");
    spk_init();
    ck("CASTSOUND=0 mutes", spk_muted() == 1);
    set_castsound("1");
    spk_init();
    ck("CASTSOUND=1 leaves sound on", spk_muted() == 0);
    set_castsound("on");
    spk_init();
    ck("CASTSOUND=on leaves sound on", spk_muted() == 0);

    printf("\n== SPK: nonsense frequencies are refused ==\n");
    fake_now = 300UL;
    spk_off();
    spk_note(0U, 5);
    ck("0 Hz does not drive the cone", !sounding);
    spk_note(50000U, 5);
    ck("50 kHz does not drive the cone", !sounding);
    spk_note(1000U, 0);
    ck("a zero-tick note is not started", !pending);

    printf("\n== SPK: spk_off() is always safe ==\n");
    spk_note(1000U, 99);
    spk_off();
    ck("spk_off silences a live note", !sounding && !pending);
    spk_off();
    ck("spk_off twice is harmless", !sounding && !pending);

    printf("\n%d checks, %d failure(s)\n", checks, failures);
    if (failures) {
        printf("SPK TESTS FAILED\n");
        return 1;
    }
    printf("ALL SPK TESTS PASSED\n");
    return 0;
}
