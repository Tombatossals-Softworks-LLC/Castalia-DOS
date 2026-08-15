/* ===================================================================
 * VIDDET.H  -  video adapter detection shared by CASTALIA tools
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Both HWINFO and CASTMARK used to ask one question - INT 10h AX=1A00h,
 * "get display combination code" - and print "VGA present" or "not
 * detected (EGA/CGA?)".  That is one probe answering for four adapter
 * generations, and when it says no it does not say what the machine
 * actually has, which is the part a user needs in order to act.
 *
 * Every probe here is a documented BIOS call or the equipment word.
 * Nothing pokes video ports: a VGA DAC read-back would be a more
 * certain test, but this is a diagnostic tool run on other people's
 * vintage hardware, and a wrong adapter name is a cosmetic fault while
 * writing to the I/O space of an unidentified card is not.
 * =================================================================== */
#ifndef CASTALIA_VIDDET_H
#define CASTALIA_VIDDET_H

enum {
    VID_UNKNOWN = 0,
    VID_MDA,            /* monochrome adapter, 80x25 text only         */
    VID_CGA,            /* colour graphics adapter                     */
    VID_EGA,            /* enhanced graphics adapter                   */
    VID_MCGA,           /* PS/2 MCGA: mode 13h but NO 16-colour EGA    */
    VID_VGA             /* VGA or better                               */
};

/* MCGA is deliberately its own answer rather than being folded into
 * VGA.  It answers the VGA-era BIOS call and reports a VGA-ish display
 * code, so the obvious test calls it VGA - but it has no EGA modes at
 * all, which for a DOS games machine is the difference between a title
 * running and not.  The display code the same call returns already
 * distinguishes them; the first version of this module was throwing
 * that byte away. */

/* Best identification the BIOS will give us. */
int vid_class(void);

/* "VGA or better", "MCGA (no EGA modes)", "EGA", "CGA", "MDA
 * (monochrome)" or "not identified". */
const char *vid_name(int cls);

/* Convenience for the many callers that only care about VGA. */
int vid_is_vga(void);

#endif /* CASTALIA_VIDDET_H */
