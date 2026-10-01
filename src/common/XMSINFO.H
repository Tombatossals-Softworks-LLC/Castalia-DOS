/* ===================================================================
 * XMSINFO.H  -  extended-memory reporting shared by CASTALIA tools
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Once an XMS manager (HIMEMX) is loaded it takes ownership of extended
 * memory and answers INT 15h AH=88h with ZERO, on purpose, so that no
 * other program helps itself to memory the manager is handing out.  A
 * tool that reports INT 15h alone therefore tells the user their 4 MB
 * machine has "0 KB extended" while simultaneously reporting that an
 * XMS driver is present - which is how it read on the first hardware
 * run.  Ask the driver instead; fall back to the BIOS only when there
 * is no driver to ask.
 * =================================================================== */
#ifndef CASTALIA_XMSINFO_H
#define CASTALIA_XMSINFO_H

/* Where a reported extended-memory figure came from. */
enum {
    XMEM_NONE = 0,      /* nobody could tell us anything            */
    XMEM_BIOS = 1,      /* INT 15h AH=88h, i.e. memory nobody owns  */
    XMEM_XMS  = 2       /* the XMS driver's own free-memory figure  */
};

/* Is an XMS manager installed?  (INT 2Fh AX=4300h) */
int xms_present(void);

/* XMS driver version as BCD, e.g. 0x0300 for 3.00.  1 on success. */
int xms_version(unsigned *bcd);

/* Free extended memory from the driver, in KB.  Either pointer may be
 * NULL.  Returns 1 on success, 0 if there is no driver or it refused.
 * A driver with every KB allocated succeeds with 0: that is an answer.
 * Note this is FREE memory, not installed memory: the manager cannot
 * tell us how much other drivers have already taken. */
int xms_free_kb(unsigned *largest, unsigned *total);

/* Best available extended-memory figure, in KB.  *source (may be NULL)
 * receives XMEM_NONE / XMEM_BIOS / XMEM_XMS so the caller can label the
 * number honestly rather than implying it measured something else. */
unsigned mem_ext_kb(int *source);

/* One-line description for a status row, e.g.
 *      "3008 KB free (XMS driver 3.00)"
 *      "3072 KB (BIOS INT 15h)"
 *      "0 KB free (XMS driver 3.00)"   (driver loaded, all allocated)
 *      "none reported (no XMS driver)"
 * `out` needs 48 bytes. */
void mem_ext_describe(char *out);

#endif /* CASTALIA_XMSINFO_H */
