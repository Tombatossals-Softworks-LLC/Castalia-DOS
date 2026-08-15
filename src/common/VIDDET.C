/* ===================================================================
 * VIDDET.C  -  video adapter detection shared by CASTALIA tools
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Probe order, most specific first.  Each one is a documented call, and
 * each is only trusted for what it can actually prove:
 *
 *   1. INT 10h AX=1A00h  "get display combination code".  Introduced
 *      with the VGA BIOS, so AL=1Ah on return means VGA-class.  A few
 *      early VGA clones omit it, which is why it is not the only test.
 *   2. INT 10h AH=1Bh    "functionality/state information".  Also
 *      VGA-only, and implemented by some clones that skip 1A00h.
 *   3. INT 10h AH=12h BL=10h  "get EGA info".  An EGA (or better) BIOS
 *      changes BL; a CGA/MDA BIOS leaves it alone, because on those
 *      machines AH=12h is not a function at all.
 *   4. The INT 11h equipment word, bits 5-4: 00 means the adapter
 *      chooses its own mode (EGA or later), 11 means MDA, and the other
 *      two are CGA at 40 or 80 columns.
 * =================================================================== */

#include <string.h>
#include <dos.h>
#include "VIDDET.H"

/* 1. The display combination code.  A BIOS old enough not to have this
 *    call leaves AL alone, so AL=1Ah is the "answered" signal; BL then
 *    names the active display outright, which is more than "is it VGA?"
 *    and is why this is the first probe.  Returns the code, or -1 when
 *    the call went unanswered.
 *
 *      01h MDA   02h CGA   04h/05h EGA   07h/08h VGA
 *      0Ah/0Bh/0Ch MCGA    06h PGC       00h none
 */
static int probe_dcc(void)
{
    union REGS r;

    r.x.ax = 0x1A00;
    int86(0x10, &r, &r);
    if (r.h.al != 0x1A)
        return -1;
    return (int)r.h.bl;
}

/* 2. VGA: functionality / state information.  Needs a 64-byte buffer
 *    the BIOS fills in; we only care that it answered at all. */
static int probe_funcinfo(void)
{
    union REGS   r;
    struct SREGS s;
    static char  buf[64];

    memset(buf, 0, sizeof(buf));
    segread(&s);
    s.es   = FP_SEG((void far *)buf);
    r.x.di = FP_OFF((void far *)buf);
    r.h.ah = 0x1B;
    r.x.bx = 0x0000;
    int86x(0x10, &r, &r, &s);
    return (r.h.al == 0x1B);
}

/* 3. EGA or better: BL comes back changed. */
static int probe_ega(void)
{
    union REGS r;

    r.h.ah = 0x12;
    r.h.bl = 0x10;
    r.h.bh = 0xFF;              /* so an untouched BH is visible too */
    int86(0x10, &r, &r);
    return (r.h.bl != 0x10 && r.h.bh != 0xFF);
}

/* 4. Equipment word, bits 5-4. */
static int equip_video(void)
{
    union REGS r;

    int86(0x11, &r, &r);
    return (int)((r.x.ax >> 4) & 3);
}

int vid_class(void)
{
    int eq, dcc = probe_dcc();

    switch (dcc) {
    case 0x01:                       return VID_MDA;
    case 0x02:                       return VID_CGA;
    case 0x04: case 0x05:            return VID_EGA;
    case 0x07: case 0x08:            return VID_VGA;
    case 0x0A: case 0x0B: case 0x0C: return VID_MCGA;
    default:   break;   /* unanswered, or a code we do not know: probe on */
    }

    if (probe_funcinfo())
        return VID_VGA;
    if (probe_ega())
        return VID_EGA;

    eq = equip_video();
    if (eq == 0)
        return VID_EGA;         /* adapter picks its own mode */
    if (eq == 3)
        return VID_MDA;
    if (eq == 1 || eq == 2)
        return VID_CGA;
    return VID_UNKNOWN;
}

const char *vid_name(int cls)
{
    switch (cls) {
    case VID_VGA:  return "VGA or better";
    case VID_MCGA: return "MCGA (no EGA modes)";
    case VID_EGA:  return "EGA";
    case VID_CGA:  return "CGA";
    case VID_MDA:  return "MDA (monochrome)";
    default:       return "not identified";
    }
}

int vid_is_vga(void)
{
    return (vid_class() == VID_VGA);
}
