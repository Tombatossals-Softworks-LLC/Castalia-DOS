/* ===================================================================
 * VIDTEST.C  -  video-adapter detection probe (VIDTEST.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Runs the shared VIDDET module against whatever adapter the machine
 * has and writes the verdict, plus every individual probe's raw answer,
 * to VIDTEST.TXT.  No UI and no keyboard: it is started from a batch
 * file inside an emulated PC and the host reads the file back.
 *
 * The point is the adapters we cannot obtain.  HWINFO's old "is it
 * VGA?" question was one BIOS call answering for four generations of
 * card, and when VIDDET replaced it only the VGA path had ever met a
 * running machine - QEMU has no CGA or EGA to offer.  DOSBox-X does:
 * `machine=cga`, `ega`, `hercules`, `vgaonly`, `svga_s3`.  This program
 * is what turns that into an assertion (scripts/test-video.sh).
 *
 * Raw probe answers are logged next to the verdict so that a
 * disagreement says WHICH probe misread the machine rather than just
 * that the answer was wrong.
 *
 * C89.  Built by the Makefile's `vidtest` target, not part of `all`.
 * =================================================================== */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include "../../src/common/VIDDET.H"

int main(void)
{
    union REGS   r;
    struct SREGS s;
    static char  buf[64];
    FILE *f;
    int   cls;
    unsigned dcc_al, dcc_bx, fi_al, ega_bl, ega_bh, eq;

    /* Raw probes, repeated here so the log records what the BIOS said
     * rather than only what VIDDET concluded from it. */
    r.x.ax = 0x1A00;
    int86(0x10, &r, &r);
    dcc_al = r.h.al;
    dcc_bx = r.x.bx;

    memset(buf, 0, sizeof(buf));
    segread(&s);
    s.es   = FP_SEG((void far *)buf);
    r.x.di = FP_OFF((void far *)buf);
    r.h.ah = 0x1B;
    r.x.bx = 0x0000;
    int86x(0x10, &r, &r, &s);
    fi_al = r.h.al;

    r.h.ah = 0x12;
    r.h.bl = 0x10;
    r.h.bh = 0xFF;
    int86(0x10, &r, &r);
    ega_bl = r.h.bl;
    ega_bh = r.h.bh;

    int86(0x11, &r, &r);
    eq = (r.x.ax >> 4) & 3;

    cls = vid_class();

    f = fopen("VIDTEST.TXT", "w");
    if (f == NULL)
        return 1;
    fprintf(f, "CLASS %d %s\n", cls, vid_name(cls));
    fprintf(f, "DCC al=%02X bx=%04X\n", dcc_al, dcc_bx);
    fprintf(f, "FUNCINFO al=%02X\n", fi_al);
    fprintf(f, "EGAINFO bl=%02X bh=%02X\n", ega_bl, ega_bh);
    fprintf(f, "EQUIP video=%u\n", eq);
    fprintf(f, "ISVGA %d\n", vid_is_vga());
    fclose(f);
    return 0;
}
