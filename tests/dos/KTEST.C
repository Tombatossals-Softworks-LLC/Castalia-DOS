/* ===================================================================
 * KTEST.C  -  Castalia kernel API probe (KTEST.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Asks the running kernel who it is and writes the answers to
 * A:\KTEST.TXT, one fact per line, then exits.  No UI, no keyboard: it
 * runs from AUTOEXEC.BAT inside an emulated PC and the host reads the
 * file back out of the floppy image afterwards.
 *
 * This is the only test that exercises the Castalia kernel
 * modifications as *code the CPU runs* rather than as source that
 * compiles.  The host gate cannot reach them (it never builds the
 * kernel), and the DOSBox boot test only proves the image reaches
 * AUTOEXEC.  See scripts/test-kernel-api.sh for the harness and
 * docs/KERNEL.md for the register contract being checked.
 *
 * Every line is written whatever happens - "ABSENT" when a call is not
 * answered - so a stock FreeDOS kernel produces a readable report
 * rather than an empty file that could be mistaken for a crash.
 *
 * C89.  Built by the Makefile's `ktest` target, not part of `all`.
 * =================================================================== */

#include <stdio.h>
#include <dos.h>

int main(void)
{
    union REGS r;
    FILE *f;
    unsigned long boot;

    f = fopen("A:\\KTEST.TXT", "w");
    if (f == NULL)
        return 1;

    /* CA00h - identity: build number, edition, OEM id. */
    r.x.ax = 0xCA00;
    int86(0x2F, &r, &r);
    if (r.h.al == 0xFF && r.x.bx == 0xCA5A)
        fprintf(f, "CA00 OK build=%d edition=%02X oem=%02X\n",
                (int)r.x.cx, (int)r.h.dh, (int)r.h.dl);
    else
        fprintf(f, "CA00 ABSENT al=%02X bx=%04X\n",
                (int)r.h.al, (unsigned)r.x.bx);

    /* CA01h - the boot profile the CASTALIA= directive recorded. */
    r.x.ax = 0xCA01;
    int86(0x2F, &r, &r);
    if (r.h.al == 0xFF && r.x.bx == 0xCA5A)
        fprintf(f, "CA01 OK profile=%d\n", (int)r.h.cl);
    else
        fprintf(f, "CA01 ABSENT\n");

    /* CA02h - the BIOS tick stamped at sign-on, for uptime. */
    r.x.ax = 0xCA02;
    int86(0x2F, &r, &r);
    if (r.h.al == 0xFF && r.x.bx == 0xCA5A) {
        boot = ((unsigned long)r.x.cx << 16) | (unsigned long)r.x.dx;
        fprintf(f, "CA02 OK boottick=%lu\n", boot);
    } else {
        fprintf(f, "CA02 ABSENT\n");
    }

    /* The standard DOS version call: BH must carry the Castalia OEM id. */
    r.h.ah = 0x30;
    int86(0x21, &r, &r);
    fprintf(f, "INT21.30 major=%d minor=%d oem(bh)=%02X\n",
            (int)r.h.al, (int)r.h.ah, (int)r.h.bh);

    fclose(f);
    return 0;
}
