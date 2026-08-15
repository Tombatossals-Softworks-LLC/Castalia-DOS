/* ===================================================================
 * CDTEST.C  -  CD-ROM / Red Book audio probe (CDTEST.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Drives CDPLAYER's own MSCDEX layer headlessly against a mounted disc
 * and writes what it found to CDTEST.TXT: the drive, the table of
 * contents, the result of starting playback, and the Q-channel position
 * a moment later.  scripts/test-cdaudio.sh runs it inside DOSBox-X with
 * a generated CUE/BIN and checks both the report and the audio that
 * came out of the emulator.
 *
 * CDPLAYER.C is #included with -DCDPLAYER_TEST rather than having its
 * driver calls copied here.  A copy would only ever prove the copy
 * works; including the file means the thing under test is the code that
 * ships.  Its main() renames itself out of the way under that macro.
 *
 * This is the one part of the CD path that emulation can reach.  What
 * it CANNOT reach is UIDE and SHSUCDX: DOSBox-X provides MSCDEX itself,
 * so the driver stack underneath this API is not the one a real 386SX
 * will use.  That still leaves everything above the API - the request
 * headers, the control blocks, the Red Book arithmetic - under test,
 * which is where the code we wrote actually lives.
 *
 * C89.  Built by the Makefile's `cdtest` target, not part of `all`.
 * =================================================================== */

#include <stdio.h>

#define CDPLAYER_TEST
#include "../../src/cdplayer/CDPLAYER.C"

int main(void)
{
    FILE *f;
    int   t;
    unsigned secs = 0, stat = 0;

    f = fopen("CDTEST.TXT", "w");
    if (f == NULL)
        return 1;

    if (!detect_cdrom()) {
        fprintf(f, "MSCDEX ABSENT\n");
        fclose(f);
        return 2;
    }
    fprintf(f, "MSCDEX OK drive=%d\n", g_drive);

    if (!read_toc()) {
        fprintf(f, "TOC FAILED\n");
        fclose(f);
        return 3;
    }
    fprintf(f, "TOC low=%d high=%d audio=%d data=%d\n",
            g_low, g_high, g_naudio, g_ndata);
    fprintf(f, "TOTAL frames=%lu\n", total_frames());
    for (t = g_low; t <= g_high; t++)
        fprintf(f, "TRACK %d start=%lu frames=%lu %s\n",
                t, rb_to_frames(g_start[t]), track_len_frames(t),
                TRACK_IS_DATA(g_ctrl[t]) ? "DATA" : "AUDIO");

    /* Play the second track: starting at the first would leave any
     * off-by-one in the Red Book arithmetic invisible. */
    t = (g_high >= g_low + 1) ? g_low + 1 : g_low;
    if (cd_play(g_start[t], track_len_frames(t)))
        fprintf(f, "PLAY OK track=%d\n", t);
    else
        fprintf(f, "PLAY FAILED track=%d\n", t);

    /* Let it run, then ask the drive where the head is and whether it
     * agrees that it is playing. */
    {
        unsigned long until = ui_ticks() + 36UL;    /* ~2 s */
        while (ui_ticks() < until)
            ;
    }
    if (cd_qchannel(&secs))
        fprintf(f, "QCHANNEL secs=%u\n", secs);
    else
        fprintf(f, "QCHANNEL FAILED\n");
    if (cd_audio_status(&stat))
        fprintf(f, "STATUS word=%04X paused=%d\n", stat, (int)(stat & 1));
    else
        fprintf(f, "STATUS FAILED\n");

    if (cd_simple(CMD_STOP_AUDIO))
        fprintf(f, "STOP OK\n");
    else
        fprintf(f, "STOP FAILED\n");

    fprintf(f, "DONE\n");
    fclose(f);
    return 0;
}
