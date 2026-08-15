/* ===================================================================
 * SPKTEST.C  -  PC-speaker tone probe (SPKTEST.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Sounds four known frequencies, separated by silence, then leaves the
 * speaker off.  No UI and no keyboard: it runs inside an emulator whose
 * audio output is captured to a file, and the host measures what came
 * out (scripts/test-speaker.sh).
 *
 * The host unit tests in tests/unit/test_spk.c cover SPK's state
 * machine thoroughly - mute, deadlines, the BIOS tick's midnight wrap -
 * but on the host the port writes compile to no-ops, so nothing there
 * proves the 8253 is being programmed correctly.  A divisor computed
 * with the wrong constant, or the gate bits set in the wrong order,
 * would pass every one of those tests and play the wrong note, or
 * nothing at all, on a real 386SX.  This is the part that only sounding
 * the tones can settle.
 *
 * The frequencies are spread across the range the suite actually uses
 * and chosen to be far apart, so the analysis does not have to be
 * clever to be conclusive.
 *
 * C89.  Built by the Makefile's `spktest` target, not part of `all`.
 * =================================================================== */

#include <stdio.h>
#include "../../src/common/SPK.H"
#include "../../src/common/UI.H"

/* Four tones, ~0.44 s each (8 BIOS ticks), 0.16 s of silence between.
 * Kept in step with scripts/test-speaker.sh, which expects this list. */
static const unsigned tones[4] = { 220u, 440u, 880u, 1760u };

static void hush(int ticks)
{
    unsigned long until = ui_ticks() + (unsigned long)ticks;
    while (ui_ticks() < until)
        ;
}

int main(void)
{
    FILE *f;
    int   i;

    /* Deliberately NOT spk_init(): that would honour CASTSOUND, and a
     * muted environment would make this program silently do nothing and
     * the test would then be asserting against its own silence. */
    spk_mute(0);

    for (i = 0; i < 4; i++) {
        spk_tone(tones[i], 8);
        spk_off();
        hush(3);
    }
    spk_off();

    f = fopen("SPKTEST.TXT", "w");
    if (f == NULL)
        return 1;
    for (i = 0; i < 4; i++)
        fprintf(f, "TONE %u\n", tones[i]);
    fprintf(f, "DONE\n");
    fclose(f);
    return 0;
}
