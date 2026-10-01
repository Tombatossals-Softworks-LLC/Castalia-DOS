/* ===================================================================
 * XMSINFO.C  -  extended-memory reporting shared by CASTALIA tools
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * BUILD NOTE: the XMS API is reached through a FAR CALL to an address
 * the driver hands back in ES:BX, not through an interrupt, so there is
 * no int86() route to it.  The three entry calls below are #pragma aux
 * stubs that call indirectly through the C variable holding that
 * address; the emitted instruction is "call dword ptr _xms_entry"
 * (verified by wdis).  The gcc CI gate compiles the #else branch, which
 * reports the BIOS figure only - it never runs on DOS.
 * =================================================================== */

#include <stdio.h>
#include <dos.h>
#include "XMSINFO.H"

/* Extended memory the BIOS admits to.  An XMS manager deliberately
 * makes this read 0 once it owns the memory. */
static unsigned bios_ext_kb(void)
{
    union REGS r;
    r.x.ax = 0x8800;
    r.h.ah = 0x88;
    int86(0x15, &r, &r);
    return r.x.ax;
}

#ifdef __WATCOMC__

/* The driver's entry point, from INT 2Fh AX=4310h.  Zero until asked. */
void (__far *xms_entry)(void) = 0;

/* AH=00h  Get version.  AX = version BCD, BX = internal revision. */
extern unsigned xms_fn_version(void);
#pragma aux xms_fn_version =            \
    "xor ah, ah"                        \
    "call dword ptr xms_entry"          \
    value [ax] modify [ax bx dx];

/* AH=08h  Query free extended memory.  AX = largest free block in KB,
 * DX = total free in KB, BL = 0 on success.  BL is loaded with 0 first
 * because XMS 3.0 drivers read it as a "which pool" selector. */
extern unsigned xms_fn_largest(void);
#pragma aux xms_fn_largest =            \
    "mov ah, 8"                         \
    "xor bl, bl"                        \
    "call dword ptr xms_entry"          \
    value [ax] modify [ax bx dx];

extern unsigned xms_fn_total(void);
#pragma aux xms_fn_total =              \
    "mov ah, 8"                         \
    "xor bl, bl"                        \
    "call dword ptr xms_entry"          \
    value [dx] modify [ax bx dx];

/* The same query once more for BL: 80h/81h mean the driver refused, A0h
 * that every KB is already allocated.  Only consulted when both sizes
 * came back 0, which is the one case the sizes cannot tell apart. */
extern unsigned xms_fn_status(void);
#pragma aux xms_fn_status =             \
    "mov ah, 8"                         \
    "xor bl, bl"                        \
    "call dword ptr xms_entry"          \
    value [bx] modify [ax bx dx];

/* Locate the driver once and cache the answer.
 * 0 = looked and found nothing, 1 = entry point ready, -1 = not asked. */
static int xms_state = -1;

static int xms_locate(void)
{
    union REGS  r;
    struct SREGS s;

    if (xms_state >= 0)
        return xms_state;

    xms_state = 0;
    r.x.ax = 0x4300;
    int86(0x2F, &r, &r);
    if (r.h.al != 0x80)
        return 0;

    segread(&s);
    r.x.ax = 0x4310;
    int86x(0x2F, &r, &r, &s);
    if (s.es == 0 && r.x.bx == 0)
        return 0;

    xms_entry = (void (__far *)(void))MK_FP(s.es, r.x.bx);
    xms_state = 1;
    return 1;
}

int xms_present(void)
{
    return xms_locate();
}

int xms_version(unsigned *bcd)
{
    unsigned v;
    if (!xms_locate())
        return 0;
    v = xms_fn_version();
    if (bcd != NULL)
        *bcd = v;
    return 1;
}

int xms_free_kb(unsigned *largest, unsigned *total)
{
    unsigned lg, tt;

    if (!xms_locate())
        return 0;

    /* Two calls rather than one because a #pragma aux stub returns a
     * single register; the query has no side effects, so asking twice
     * is safe and keeps the stubs trivial. */
    lg = xms_fn_largest();
    tt = xms_fn_total();

    /* Both 0 is either a refusal (an error code in BL) or a driver that
     * really has nothing left - every KB handed out, BL = A0h.  The
     * second is a true answer, "0 KB free", and must not fall through
     * to INT 15h, which reads 0 under the driver and would turn it into
     * "none reported".  A refusal still means we have no figure. */
    if (lg == 0 && tt == 0) {
        unsigned bl = xms_fn_status() & 0xFFu;
        if (bl != 0x00u && bl != 0xA0u)
            return 0;
    }

    if (largest != NULL) *largest = lg;
    if (total   != NULL) *total   = tt;
    return 1;
}

#else  /* host gcc syntax gate: no far calls, BIOS figure only */

int xms_present(void)
{
    union REGS r;
    r.x.ax = 0x4300;
    int86(0x2F, &r, &r);
    return (r.h.al == 0x80);
}

int xms_version(unsigned *bcd)
{
    (void)bcd;
    return 0;
}

int xms_free_kb(unsigned *largest, unsigned *total)
{
    (void)largest;
    (void)total;
    return 0;
}

#endif /* __WATCOMC__ */

unsigned mem_ext_kb(int *source)
{
    unsigned total = 0, bios;

    /* A driver that answers is the authority even when it answers 0:
     * the BIOS figure under it is 0 by design, not a second opinion. */
    if (xms_free_kb(NULL, &total)) {
        if (source != NULL) *source = XMEM_XMS;
        return total;
    }
    bios = bios_ext_kb();
    if (bios > 0) {
        if (source != NULL) *source = XMEM_BIOS;
        return bios;
    }
    if (source != NULL) *source = XMEM_NONE;
    return 0;
}

void mem_ext_describe(char *out)
{
    unsigned kb, bcd = 0;
    int src = XMEM_NONE;

    kb = mem_ext_kb(&src);

    switch (src) {
    case XMEM_XMS:
        if (xms_version(&bcd) && bcd != 0)
            sprintf(out, "%u KB free (XMS driver %x.%02x)",
                    kb, (bcd >> 8) & 0xFF, bcd & 0xFF);
        else
            sprintf(out, "%u KB free (XMS driver)", kb);
        break;
    case XMEM_BIOS:
        sprintf(out, "%u KB (BIOS INT 15h)", kb);
        break;
    default:
        if (xms_present())
            sprintf(out, "XMS driver present, no free memory reported");
        else
            sprintf(out, "none reported (no XMS driver)");
        break;
    }
}
