/* ===================================================================
 * CASTDOC.C  -  CASTALIA DISK DOCTOR: media inspector & surface scan
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * The Castalia answer to a disk doctor's SURFACE side: read-only,
 * BIOS-level (INT 13h) media inspection and sector verification for
 * floppies and hard disks, with a live track map, an error-code
 * breakdown, and plain-language advice that helps you tell a failing
 * DISKETTE from a failing DRIVE.
 *
 * Scope and honesty:
 *   - 100% NON-DESTRUCTIVE: only INT 13h AH=04h (verify) is used.  It
 *     never writes, never "repairs".
 *   - FAT logical-structure checking stays with FreeDOS CHKDSK; this
 *     tool is about the physical surface and the drive.
 *   - Floppies get a FULL verify of every track.  Hard disks default to
 *     a SAMPLED scan (first+last head of cylinder groups), clearly
 *     labelled; a full HDD scan is available but slow.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os castdoc.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include "../common/UI.H"

#ifndef MK_FP
#define MK_FP(seg, ofs) \
    ((void far *)(((unsigned long)(seg) << 16) | (unsigned)(ofs)))
#endif

/* Verify buffer: some BIOSes want ES:BX valid even for AH=04h. */
static char vbuf[9216];             /* 18 sectors x 512 bytes */

/* --- INT 13h primitives ------------------------------------------------ */

static int bios_reset(int drv)
{
    union REGS r;
    struct SREGS s;
    segread(&s);
    r.h.ah = 0x00;
    r.h.dl = (unsigned char)drv;
    int86x(0x13, &r, &r, &s);
    return r.h.ah;
}

static int bios_getparam(int drv, int *cyls, int *heads, int *spt, int *type)
{
    union REGS r;
    struct SREGS s;
    segread(&s);
    r.h.ah = 0x08;
    r.h.dl = (unsigned char)drv;
    r.x.bx = 0;
    r.x.cx = 0;
    r.x.dx = 0;
    int86x(0x13, &r, &r, &s);
    if (r.x.cflag || r.x.cx == 0)
        return -1;
    *spt   = (int)(r.h.cl & 0x3F);
    *cyls  = ((int)r.h.ch | (((int)r.h.cl & 0xC0) << 2)) + 1;
    *heads = (int)r.h.dh + 1;
    *type  = (int)r.h.bl;
    return 0;
}

/* Verify 'count' sectors at CHS.  0 = OK, else BIOS status code. */
static int bios_verify(int drv, int cyl, int head, int sec, int count)
{
    union REGS r;
    struct SREGS s;
    segread(&s);
    s.es   = FP_SEG((void far *)vbuf);
    r.x.bx = FP_OFF((void far *)vbuf);
    r.h.ah = 0x04;
    r.h.al = (unsigned char)count;
    r.h.ch = (unsigned char)(cyl & 0xFF);
    r.h.cl = (unsigned char)((sec & 0x3F) | ((cyl >> 2) & 0xC0));
    r.h.dh = (unsigned char)head;
    r.h.dl = (unsigned char)drv;
    int86x(0x13, &r, &r, &s);
    if (r.x.cflag)
        return r.h.ah ? (int)r.h.ah : 0xFF;
    return 0;
}

/* Verify one whole track with reset+retry.  0 = OK, else last status. */
static int verify_track(int drv, int cyl, int head, int spt)
{
    int rc, attempt;
    for (attempt = 0; attempt < 3; attempt++) {
        rc = bios_verify(drv, cyl, head, 1, spt);
        if (rc == 0)
            return 0;
        bios_reset(drv);
    }
    return rc;
}

static const char *status_name(int code)
{
    switch (code) {
    case 0x01: return "invalid command";
    case 0x02: return "address mark not found";
    case 0x03: return "write-protected";
    case 0x04: return "sector not found";
    case 0x06: return "media changed";
    case 0x08: return "DMA overrun";
    case 0x10: return "CRC/data error";
    case 0x20: return "controller failure";
    case 0x40: return "seek failure";
    case 0x80: return "timeout / not ready";
    default:   return "other error";
    }
}

static const char *floppy_type_name(int bl)
{
    switch (bl) {
    case 1: return "360 KB 5.25\"";
    case 2: return "1.2 MB 5.25\"";
    case 3: return "720 KB 3.5\"";
    case 4: return "1.44 MB 3.5\"";
    case 5:
    case 6: return "2.88 MB 3.5\"";
    default: return "unknown type";
    }
}

/* --- Error histogram ---------------------------------------------------- */

#define MAXCODES 8
static int err_code[MAXCODES];
static int err_count[MAXCODES];
static int n_codes = 0;

static void err_note(int code)
{
    int i;
    for (i = 0; i < n_codes; i++) {
        if (err_code[i] == code) {
            err_count[i]++;
            return;
        }
    }
    if (n_codes < MAXCODES) {
        err_code[n_codes] = code;
        err_count[n_codes] = 1;
        n_codes++;
    }
}

/* --- Screens -------------------------------------------------------------- */

static void title_bar(void)
{
    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA DISK DOCTOR", A_TITLE);
    ui_puts(24, 0, "Surface Verify (read-only)", UI_ATTR(C_WHITE, C_BLUE));
}

static void advise_box(int bad, int isfloppy)
{
    int w = 74, h = 9, x = 3, y = 14;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    if (bad == 0) {
        ui_puts(x + 2, y, " Verdict: CLEAN ", A_PANELHDR);
        ui_puts(x + 3, y + 2, "Every verified track reported no errors.",
                A_PANEL);
        ui_puts(x + 3, y + 4,
                "The media surface and the drive read chain look healthy.",
                A_PANEL);
    } else if (isfloppy) {
        ui_puts(x + 2, y, " Verdict: ERRORS FOUND ", A_PANELHDR);
        ui_puts(x + 3, y + 2,
                "To tell the DISKETTE from the DRIVE, scan a second,",
                A_PANEL);
        ui_puts(x + 3, y + 3,
                "known-good diskette in this same drive:", A_PANEL);
        ui_puts(x + 3, y + 4,
                " - that one fails too -> suspect the DRIVE (dirty heads;",
                A_PANEL);
        ui_puts(x + 3, y + 5,
                "   try a head-cleaning kit, then a different drive).",
                A_PANEL);
        ui_puts(x + 3, y + 6,
                " - only this disk fails -> the DISKETTE is degrading.",
                A_PANEL);
        ui_puts(x + 3, y + 7,
                "   Rescue its files NOW with CASTCOPY and retire it.",
                A_PANEL);
    } else {
        ui_puts(x + 2, y, " Verdict: ERRORS FOUND ", A_PANELHDR);
        ui_puts(x + 3, y + 2,
                "This hard disk reported verify errors.  Back up your",
                A_PANEL);
        ui_puts(x + 3, y + 3,
                "data first.  Re-run the scan: growing error counts over",
                A_PANEL);
        ui_puts(x + 3, y + 4,
                "time mean the disk is failing; a stable small set may",
                A_PANEL);
        ui_puts(x + 3, y + 5,
                "be old factory defects already mapped out by the FAT.",
                A_PANEL);
        ui_puts(x + 3, y + 6,
                "Run FreeDOS CHKDSK for the logical (FAT) side.", A_PANEL);
    }
}

/* --- Scans ------------------------------------------------------------------ */

/* Returns bad-track count, or -1 when aborted / drive not ready. */
static int scan_floppy(int drv, int cyls, int heads, int spt)
{
    int cyl, head, bad = 0, rc;
    int x0 = (SCR_W - cyls) / 2;
    char buf[64];

    ui_puts(3, 8, "Head 0:", A_ITEM);
    ui_puts(3, 10, "Head 1:", A_ITEM);
    for (head = 0; head < heads && head < 2; head++)
        for (cyl = 0; cyl < cyls; cyl++)
            ui_putc(x0 + cyl, 8 + head * 2, (char)0xB0, A_HINT);

    /* Not-ready pre-check on the first track. */
    rc = verify_track(drv, 0, 0, spt);
    if (rc == 0x80 || rc == 0x40) {
        sprintf(buf, "Drive not ready (%s). Insert a diskette.",
                status_name(rc));
        ui_puts(3, 12, buf, A_WARN);
        return -1;
    }

    for (cyl = 0; cyl < cyls; cyl++) {
        for (head = 0; head < heads && head < 2; head++) {
            if (ui_keywaiting()) {
                if (ui_getkey() == KEY_ESC) {
                    ui_puts(3, 12, "Scan cancelled.                    ",
                            A_WARN);
                    return -1;
                }
            }
            rc = (cyl == 0 && head == 0) ? rc
                : verify_track(drv, cyl, head, spt);
            if (rc == 0) {
                ui_putc(x0 + cyl, 8 + head * 2, (char)0xDB,
                        UI_ATTR(C_LGREEN, C_BLUE));
            } else {
                bad++;
                err_note(rc);
                ui_putc(x0 + cyl, 8 + head * 2, 'B',
                        UI_ATTR(C_WHITE, C_RED));
            }
            sprintf(buf, "Cyl %2d  Head %d  -  %d bad track(s)   ",
                    cyl, head, bad);
            ui_puts(3, 12, buf, A_ITEM);
        }
    }
    return bad;
}

/* Sampled or full HDD scan mapped onto 2 rows x 70 cylinder-group cells.
 * Returns bad-cell count, or -1 when aborted. */
static int scan_hdd(int drv, int cyls, int heads, int spt, int full)
{
    int cells = 140, cell, bad = 0;
    long c0, c1, cyl;
    int head, rc, cellbad;
    char buf[70];

    ui_puts(3, 8, "outer:", A_ITEM);
    ui_puts(3, 10, "inner:", A_ITEM);
    for (cell = 0; cell < cells; cell++)
        ui_putc(10 + (cell % 70), 8 + (cell / 70) * 2, (char)0xB0, A_HINT);

    for (cell = 0; cell < cells; cell++) {
        c0 = (long)cyls * cell / cells;
        c1 = (long)cyls * (cell + 1) / cells;
        if (c1 <= c0)
            c1 = c0 + 1;
        cellbad = 0;

        for (cyl = c0; cyl < c1 && cyl < (long)cyls; cyl++) {
            for (head = 0; head < heads; head++) {
                if (!full) {
                    /* Sampled: first cylinder of the group only,
                     * first and last head. */
                    if (cyl != c0 || (head != 0 && head != heads - 1))
                        continue;
                }
                if (ui_keywaiting()) {
                    if (ui_getkey() == KEY_ESC) {
                        ui_puts(3, 12, "Scan cancelled.                 ",
                                A_WARN);
                        return -1;
                    }
                }
                rc = verify_track(drv, (int)cyl, head, spt);
                if (rc != 0) {
                    cellbad = 1;
                    err_note(rc);
                }
            }
            if (!full)
                break;
        }
        if (cellbad) {
            bad++;
            ui_putc(10 + (cell % 70), 8 + (cell / 70) * 2, 'B',
                    UI_ATTR(C_WHITE, C_RED));
        } else {
            ui_putc(10 + (cell % 70), 8 + (cell / 70) * 2, (char)0xDB,
                    UI_ATTR(C_LGREEN, C_BLUE));
        }
        sprintf(buf, "Cylinders %ld-%ld  -  %d bad zone(s)   ",
                c0, c1 - 1, bad);
        ui_puts(3, 12, buf, A_ITEM);
    }
    return bad;
}

static void show_errors_summary(void)
{
    int i, y = 23;
    char buf[78];
    if (n_codes == 0)
        return;
    buf[0] = '\0';
    strcat(buf, "Errors: ");
    for (i = 0; i < n_codes; i++) {
        char one[32];
        sprintf(one, "%s x%d  ", status_name(err_code[i]), err_count[i]);
        if (strlen(buf) + strlen(one) >= sizeof(buf))
            break;                      /* bounded: never overflow buf */
        strcat(buf, one);
    }
    ui_putlim(2, y, buf, 76, A_WARN);
}

/* --- Per-drive flow ------------------------------------------------------------ */

static void inspect_drive(int drv, const char *label, int isfloppy)
{
    int cyls, heads, spt, type;
    int bad, key, full = 0;
    char buf[78];
    long total_kb;

    n_codes = 0;

    title_bar();
    sprintf(buf, " %s ", label);
    ui_box(1, 2, 78, 21, A_FRAME);
    ui_puts(3, 2, buf, A_TITLE);

    if (bios_getparam(drv, &cyls, &heads, &spt, &type) != 0) {
        ui_puts(3, 4, "The BIOS reports no such drive.", A_WARN);
        ui_puts(3, 6, "Press any key to return.", A_HINT);
        ui_getkey();
        return;
    }

    total_kb = (long)cyls * heads * spt / 2;
    if (isfloppy)
        sprintf(buf, "Type: %s   Geometry: %d cyl x %d heads x %d spt"
                "   (%ld KB)",
                floppy_type_name(type), cyls, heads, spt, total_kb);
    else
        sprintf(buf, "Geometry (BIOS CHS): %d cyl x %d heads x %d spt"
                "   (~%ld MB)",
                cyls, heads, spt, total_kb / 1024L);
    ui_putlim(3, 4, buf, 74, UI_ATTR(C_YELLOW, C_BLUE));

    if (isfloppy)
        ui_puts(3, 6, "ENTER: full surface verify (every track, read-only)"
                "   Esc: back", A_ITEM);
    else
        ui_puts(3, 6, "ENTER: sampled scan   F: FULL scan (slow)   Esc: back",
                A_ITEM);

    for (;;) {
        key = ui_getkey();
        if (key == KEY_ESC)
            return;
        if (key == KEY_ENTER) { full = 0; break; }
        if (!isfloppy && (key == 'f' || key == 'F')) { full = 1; break; }
    }

    ui_puts(3, 6, "Scanning...  Esc cancels.  Legend: "
            "green = OK, red B = errors      ", A_HINT);

    if (isfloppy)
        bad = scan_floppy(drv, cyls, heads, spt);
    else
        bad = scan_hdd(drv, cyls, heads, spt, full);

    if (bad >= 0) {
        if (!isfloppy && !full)
            ui_puts(3, 13, "(sampled scan: spot checks per cylinder group)",
                    A_HINT);
        advise_box(bad, isfloppy);
        show_errors_summary();
    }
    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(2, SCR_H - 1, " Press any key to return.", A_STATUS);
    ui_getkey();
}

/* --- Drive menu ------------------------------------------------------------------ */

static int floppy_count(void)
{
    union REGS r;
    int86(0x11, &r, &r);
    if (!(r.x.ax & 0x0001))
        return 0;
    return (int)(((r.x.ax >> 6) & 3) + 1);
}

static int hdd_count(void)
{
    /* BIOS data area 0040:0075 = number of fixed disks. */
    unsigned char far *n = (unsigned char far *)MK_FP(0x0040, 0x0075);
    return (int)*n;
}

int main(void)
{
    int sel = 0, key, i;
    int nfd = floppy_count();
    int nhd = hdd_count();
    static const char *items[3] = {
        "Floppy A:  (BIOS drive 00h)",
        "Floppy B:  (BIOS drive 01h)",
        "Hard disk 1  (BIOS drive 80h)"
    };
    int enabled[3];

    enabled[0] = (nfd >= 1);
    enabled[1] = (nfd >= 2);
    enabled[2] = (nhd >= 1);

    ui_init();
    for (;;) {
        title_bar();
        ui_center(4, "Choose a drive to inspect", A_TITLE);
        ui_box(22, 6, 36, 5, A_FRAME);
        for (i = 0; i < 3; i++) {
            unsigned char a = (i == sel) ? A_ITEMSEL
                              : enabled[i] ? A_ITEM : A_HINT;
            char buf[36];
            sprintf(buf, " %-32.32s", items[i]);
            ui_fill(23, 7 + i, 34, 1, ' ', a);
            ui_puts(23, 7 + i, buf, a);
        }
        ui_center(13, "Verification is READ-ONLY: nothing is ever "
                  "written to the media.", A_HINT);
        ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
        ui_puts(2, SCR_H - 1,
                " Up/Down Select   Enter Inspect   Esc Exit", A_STATUS);

        key = ui_getkey();
        if (key == KEY_ESC)
            break;
        else if (key == KEY_UP)
            sel = (sel > 0) ? sel - 1 : 2;
        else if (key == KEY_DOWN)
            sel = (sel < 2) ? sel + 1 : 0;
        else if (key == KEY_ENTER) {
            if (!enabled[sel]) {
                /* Not reported by the BIOS; say so politely. */
                ui_center(15, "The BIOS does not report that drive.",
                          A_WARN);
                ui_getkey();
            } else if (sel == 0) {
                inspect_drive(0x00, "Floppy A:", 1);
            } else if (sel == 1) {
                inspect_drive(0x01, "Floppy B:", 1);
            } else {
                inspect_drive(0x80, "Hard disk 1", 0);
            }
        }
    }

    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
