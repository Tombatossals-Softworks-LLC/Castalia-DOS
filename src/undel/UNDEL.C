/* ===================================================================
 * UNDEL.C  -  CASTALIA UNDELETE: read-only deleted-file recovery
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * THE RULE THAT DEFINES THIS TOOL
 * -------------------------------
 * UNDEL NEVER WRITES TO THE DISK IT IS RECOVERING FROM.  Not one byte.
 * Ever.  The source volume is only ever read, through DOS's read-only
 * absolute-sector interface; there is no write path to it anywhere in
 * this file.
 *
 * Classic undelete tools "repair" the damaged disk in place: they patch
 * the directory entry and the FAT of the very volume whose data you are
 * trying to save.  One bug there and the data is gone for good.  So
 * Castalia recovers the way CASTCOPY rescues diskettes: it reads the
 * deleted file's clusters and COPIES them out to a DIFFERENT drive that
 * you choose.  A destination on the source drive is refused, because
 * writing there could land on the very clusters still holding your
 * deleted file.
 *
 * NO UNFORMAT.  Unformat has to rebuild the boot sector, the FAT and the
 * root directory OF THE DAMAGED DISK - it cannot be done without writing
 * to it.  By the rule above that is out of scope for this tool, and it
 * always will be.  Nothing here rebuilds, repairs or "recovers in place".
 *
 * HONEST LIMITATIONS (stated in the UI too, not just here)
 * --------------------------------------------------------
 *  - DOS throws the cluster chain away when it deletes a file: the FAT
 *    entries are simply set to 0.  All that survives is the START
 *    cluster in the directory entry and the size.  So a recovered file
 *    can only be reconstructed by assuming it was CONTIGUOUS.  That is
 *    the normal case for a file written once to a roomy disk, and it
 *    usually works.  A FRAGMENTED file CANNOT be rebuilt this way, by
 *    anyone, and UNDEL will not pretend otherwise.
 *  - DOS also overwrites the first character of the name with 0xE5.
 *    That letter is gone.  UNDEL puts '_' in its place and says so, so
 *    nobody thinks the tool mangled the name.
 *  - Only the ROOT directory is scanned in this release.
 *  - FAT12 and FAT16 with 512-byte sectors only.  Anything else is
 *    reported plainly rather than guessed at.
 *
 * HOW THE DISK IS READ (and why it is safe from C)
 * -------------------------------------------------
 * The obvious tool is INT 25h (absolute disk read) - read-only by
 * definition.  It is NOT used here, because INT 25h leaves a flags word
 * pushed on the stack that the caller must pop ("add sp,2"); from C via
 * int86()/int86x() there is no way to do that, and a leaked stack word
 * per call corrupts the stack.  A disk rescue tool must not gamble the
 * machine's stack.  So UNDEL uses, in this order:
 *   1. INT 21h AX=7305h (DOS 7.x / FreeDOS extended absolute DISKREAD).
 *      Same read-only service, a normal INT 21h call: it returns with
 *      the stack balanced and is perfectly C-callable.  SI bit 0 selects
 *      read (0) or write (1) - we hard-code SI=0 and never touch it.
 *   2. INT 21h AX=440Dh CX=0861h (generic IOCTL, "read track on logical
 *      drive") if 7305h is unavailable on an older kernel.  Also a plain
 *      INT 21h call, also read-only, also stack-clean; it wants CHS
 *      relative to the volume, which we derive from the BPB geometry.
 * Both are read services.  There is no call to their write twins (7305h
 * with SI bit 0 set, or IOCTL minor 41h) anywhere in this file.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os undel.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.  The real build has a 16-BIT int, so
 * every sector count, cluster number, byte count and offset below is
 * unsigned / unsigned long - an int would overflow at 32767 long before
 * a FAT16 volume runs out of sectors.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include "../common/UI.H"

#define MAX_DEL     200         /* deleted entries the list can hold      */
#define SECSIZE     512u        /* the only sector size we support        */
#define IOSECS      16u         /* sectors per absolute read (8 KB)       */
#define NAMELEN     13
#define NOTELEN     40
#define BADLBA      0xFFFFFFFFUL

/* Recoverability verdicts. */
#define V_NONE      0           /* nothing to recover                     */
#define V_GOOD      1           /* contiguous and untouched               */
#define V_DOUBT     2           /* recoverable, but with a caveat         */
#define V_OVER      3           /* clusters reused by another file        */
#define V_ERR       4           /* directory entry does not add up        */

typedef struct {
    char          name[NAMELEN];        /* "_AME.EXT" - see header        */
    char          note[NOTELEN];        /* verdict, in plain words        */
    unsigned long size;
    unsigned long need;                 /* clusters the size implies      */
    unsigned long taken;                /* how many are in use again      */
    unsigned      first;                /* start cluster                  */
    unsigned      date;
    unsigned      time;
    unsigned char attr;
    int           verdict;
    int           tag;
} DENT;

/* --- Static state (no dynamic allocation anywhere) --------------------- */

static DENT dents[MAX_DEL];
static int  n_del = 0;              /* entries stored                     */
static int  n_found = 0;            /* entries seen (may exceed n_del)    */
static int  n_badsec = 0;           /* root sectors that would not read   */

static unsigned char secbuf[SECSIZE];           /* boot / directory sector */
static unsigned char fatbuf[SECSIZE * 2u];      /* 2-sector FAT window     */
static unsigned char iobuf[SECSIZE * IOSECS];   /* file data in flight     */
static unsigned char pkt[16];                   /* INT 21h parameter block */

static int  src_drive = 0;          /* 0 = A:, 1 = B:, 2 = C: ...         */
static char dest[80] = "C:\\RESCUE";

/* Volume geometry, all derived from the BPB in the boot sector.  Every
 * one of these is deliberately wider than an int. */
static unsigned      bytes_per_sec = 0;
static unsigned      sec_per_clus  = 0;
static unsigned long clus_bytes    = 0;   /* up to 128*512 = 65536: LONG! */
static unsigned long fat_start     = 0;
static unsigned long root_start    = 0;
static unsigned long root_secs     = 0;
static unsigned long data_start    = 0;
static unsigned long total_secs    = 0;
static unsigned long total_clus    = 0;
static unsigned long max_clus      = 0;   /* highest valid cluster number */
static unsigned      root_ents     = 0;
static unsigned      geo_spt       = 0;   /* BPB sectors per track        */
static unsigned      geo_heads     = 0;   /* BPB heads                    */
static int           is_fat12      = 0;
static int           read_mode     = 0;   /* 0 = INT 21h 7305h, 1 = IOCTL */

static unsigned long fatbuf_lba = BADLBA; /* first sector cached in fatbuf */
static int           fat_err    = 0;      /* a FAT read failed             */

/* --- Little-endian field access (no struct packing assumptions) -------- */

static unsigned get16(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned long get32(const unsigned char *p)
{
    return (unsigned long)p[0]
         | ((unsigned long)p[1] << 8)
         | ((unsigned long)p[2] << 16)
         | ((unsigned long)p[3] << 24);
}

static void put16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)(v & 0xFFu);
    p[1] = (unsigned char)((v >> 8) & 0xFFu);
}

static void put32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v & 0xFFUL);
    p[1] = (unsigned char)((v >> 8) & 0xFFUL);
    p[2] = (unsigned char)((v >> 16) & 0xFFUL);
    p[3] = (unsigned char)((v >> 24) & 0xFFUL);
}

/* --- UI helpers -------------------------------------------------------- */

/* Blocking key wait that idles politely instead of spinning the CPU. */
static int wait_key(void)
{
    for (;;) {
        if (ui_keywaiting())
            return ui_getkey();
        ui_idle();
    }
}

/* Non-blocking Esc check for progress loops.  1 = the user pressed Esc. */
static int abort_pressed(void)
{
    ui_idle();
    if (ui_keywaiting()) {
        if (ui_getkey() == KEY_ESC)
            return 1;
    }
    return 0;
}

static void notify(const char *l1, const char *l2, const char *l3)
{
    int w = 66, h = 8, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    if (l1) ui_putlim(x + 3, y + 2, l1, w - 6, A_PANEL);
    if (l2) ui_putlim(x + 3, y + 3, l2, w - 6, A_PANEL);
    if (l3) ui_putlim(x + 3, y + 4, l3, w - 6, A_PANEL);
    ui_puts(x + 3, y + 6, "Press any key.", A_HINT);
    wait_key();
}

/* Scale a byte count to 0..1000 without overflowing a 32-bit long. */
static int permille(unsigned long done, unsigned long total)
{
    unsigned long d = done, t = total;
    while (t > 4000000UL) {         /* keep d*1000 inside 32 bits */
        t >>= 4;
        d >>= 4;
    }
    if (t == 0UL)
        return 0;
    if (d > t)
        d = t;
    return (int)(d * 1000UL / t);
}

/* --- DOS drive helpers ------------------------------------------------- */

static int cur_drive(void)          /* 0 = A: */
{
    union REGS r;
    memset(&r, 0, sizeof(r));
    r.h.ah = 0x19;
    int86(0x21, &r, &r);
    return (int)r.h.al;
}

static int floppy_count(void)
{
    union REGS r;
    memset(&r, 0, sizeof(r));
    int86(0x11, &r, &r);
    if (!(r.x.ax & 0x0001u))
        return 0;
    return (int)(((r.x.ax >> 6) & 3u) + 1u);
}

/* 0 = no such drive, 1 = local removable, 2 = local fixed, 3 = network. */
static int drive_kind(int d)
{
    union REGS r;

    memset(&r, 0, sizeof(r));
    r.x.ax = 0x4409;                /* IOCTL: is drive remote?            */
    r.h.bl = (unsigned char)(d + 1);
    int86(0x21, &r, &r);
    if (r.x.cflag)
        return 0;
    if (r.x.dx & 0x1000u)
        return 3;                   /* network/CD redirector: no sectors  */

    memset(&r, 0, sizeof(r));
    r.x.ax = 0x4408;                /* IOCTL: is media removable?         */
    r.h.bl = (unsigned char)(d + 1);
    int86(0x21, &r, &r);
    if (r.x.cflag)
        return 2;
    return (r.x.ax == 0) ? 1 : 2;
}

/* MKDIR on the DESTINATION drive only (never on the source - the caller
 * has already refused a same-drive destination).  0 on success. */
static int dos_mkdir(char *path)
{
    union REGS r;
    struct SREGS s;
    memset(&r, 0, sizeof(r));
    segread(&s);
    s.ds   = FP_SEG((void far *)path);
    r.x.dx = FP_OFF((void far *)path);
    r.h.ah = 0x39;
    int86x(0x21, &r, &r, &s);
    return r.x.cflag ? (int)r.x.ax : 0;
}

/* --- READ-ONLY sector access ------------------------------------------- */

static unsigned long far_addr(void far *p)
{
    return ((unsigned long)FP_SEG(p) << 16) | (unsigned long)FP_OFF(p);
}

/* INT 21h AX=7305h - extended absolute disk READ (DOS 7.x / FreeDOS).
 * DS:BX -> DISKIO packet, CX=FFFFh, DL = drive (1=A), SI = mode.
 * SI bit 0 is the read/write selector; it is hard-wired to 0 here and
 * this tool has no code path that ever sets it. */
static int read_7305(unsigned long lba, unsigned count, unsigned char *buf)
{
    union REGS r;
    struct SREGS s;

    put32(pkt + 0, lba);            /* DWORD starting sector   */
    put16(pkt + 4, count);          /* WORD  sector count      */
    put32(pkt + 6, far_addr((void far *)buf));  /* DWORD buffer */

    memset(&r, 0, sizeof(r));
    segread(&s);
    s.ds   = FP_SEG((void far *)pkt);
    r.x.bx = FP_OFF((void far *)pkt);
    r.x.ax = 0x7305;
    r.x.cx = 0xFFFF;
    r.x.si = 0x0000;                /* READ.  Never anything else.        */
    r.h.dl = (unsigned char)(src_drive + 1);
    int86x(0x21, &r, &r, &s);
    if (r.x.cflag)
        return (int)(r.x.ax ? r.x.ax : 0xFFu);
    return 0;
}

/* INT 21h AX=440Dh CX=0861h - generic IOCTL "read track on logical
 * drive".  One call may not cross a track boundary. */
static int ioctl_track(unsigned long cyl, unsigned head, unsigned sec,
                       unsigned n, unsigned char *buf)
{
    union REGS r;
    struct SREGS s;

    if (cyl > 0xFFFFUL)
        return 0xFF;
    pkt[0] = 0;                     /* special functions: must be zero    */
    put16(pkt + 1, head);
    put16(pkt + 3, (unsigned)cyl);
    put16(pkt + 5, sec);            /* 0-based sector within the track    */
    put16(pkt + 7, n);
    put32(pkt + 9, far_addr((void far *)buf));

    memset(&r, 0, sizeof(r));
    segread(&s);
    s.ds   = FP_SEG((void far *)pkt);
    r.x.dx = FP_OFF((void far *)pkt);
    r.x.ax = 0x440D;
    r.x.cx = 0x0861;                /* category 08 disk, minor 61h = READ */
    r.h.bl = (unsigned char)(src_drive + 1);
    int86x(0x21, &r, &r, &s);
    if (r.x.cflag)
        return (int)(r.x.ax ? r.x.ax : 0xFFu);
    return 0;
}

/* LBA -> volume-relative CHS using the BPB geometry, split at track ends.
 * Before the BPB is known we can still fetch LBA 0, because sector 0 is
 * cylinder 0 / head 0 / sector 0 under ANY geometry. */
static int read_ioctl(unsigned long lba, unsigned count, unsigned char *buf)
{
    unsigned long left = (unsigned long)count;
    unsigned long track, cyl, rem;
    unsigned head, sec, n;
    int rc;

    if (geo_spt == 0 || geo_heads == 0) {
        if (lba != 0UL || count != 1u)
            return 0xFF;
        return ioctl_track(0UL, 0u, 0u, 1u, buf);
    }
    track = (unsigned long)geo_spt * (unsigned long)geo_heads;
    while (left > 0UL) {
        cyl  = lba / track;
        rem  = lba % track;
        head = (unsigned)(rem / (unsigned long)geo_spt);
        sec  = (unsigned)(rem % (unsigned long)geo_spt);
        n    = geo_spt - sec;                   /* to the end of the track */
        if ((unsigned long)n > left)
            n = (unsigned)left;
        rc = ioctl_track(cyl, head, sec, n, buf);
        if (rc)
            return rc;
        buf  += (unsigned)n * bytes_per_sec;    /* <= 8 KB, stays in range */
        lba  += (unsigned long)n;
        left -= (unsigned long)n;
    }
    return 0;
}

/* The one and only way this tool touches the source disk.  READ ONLY. */
static int disk_read(unsigned long lba, unsigned count, unsigned char *buf)
{
    if (read_mode == 0)
        return read_7305(lba, count, buf);
    return read_ioctl(lba, count, buf);
}

/* Does secbuf hold something shaped like a boot sector?  Used only to
 * decide whether the interface we tried actually delivered the sector -
 * parse_bpb does the real, strict validation afterwards. */
static int looks_like_bpb(void)
{
    unsigned bps = get16(secbuf + 11);
    if (bps != 512u && bps != 1024u && bps != 2048u && bps != 4096u)
        return 0;
    if (secbuf[13] == 0 || secbuf[16] == 0)
        return 0;
    return 1;
}

/* Pick a working read interface by fetching the boot sector with it.
 * Returns 0 on success (boot sector left in secbuf), else an error code. */
static int disk_open(void)
{
    int rc0, rc1;

    fatbuf_lba = BADLBA;
    geo_spt    = 0;                 /* forget the previous volume's shape */
    geo_heads  = 0;

    read_mode = 0;
    rc0 = read_7305(0UL, 1u, secbuf);
    if (rc0 == 0 && looks_like_bpb())
        return 0;
    /* Either 7305h is missing (older kernel) or it handed back something
     * that is not a boot sector; try the IOCTL path before giving up. */
    read_mode = 1;
    rc1 = read_ioctl(0UL, 1u, secbuf);
    if (rc1 == 0)
        return 0;
    if (rc0 == 0) {                 /* 7305h did read *something*: keep it
                                     * and let parse_bpb say what it is. */
        read_mode = 0;
        return read_7305(0UL, 1u, secbuf);
    }
    return rc0;
}

/* --- BPB arithmetic ----------------------------------------------------
 * Offsets in the boot sector, all little-endian:
 *   11 WORD  bytes per sector      13 BYTE  sectors per cluster
 *   14 WORD  reserved sectors      16 BYTE  number of FATs
 *   17 WORD  root directory entries
 *   19 WORD  total sectors (0 => use the 32-bit field at 32)
 *   22 WORD  sectors per FAT       24 WORD  sectors per track
 *   26 WORD  heads                 32 DWORD total sectors (large)
 * Layout of the volume:
 *   FAT area   starts at  reserved
 *   root dir   starts at  reserved + FATs * sectorsPerFat
 *   data area  starts at  rootStart + rootEntries*32 / bytesPerSector
 * FAT12 vs FAT16 is decided by the CLUSTER COUNT, never by the boot
 * sector's ASCII "FAT12   " label, which is only a hint. */
static int parse_bpb(char *why)
{
    unsigned nfats, spf;
    unsigned long fat_area, data_secs, t16, t32;

    bytes_per_sec = get16(secbuf + 11);
    sec_per_clus  = (unsigned)secbuf[13];
    nfats         = (unsigned)secbuf[16];
    root_ents     = get16(secbuf + 17);
    spf           = get16(secbuf + 22);
    geo_spt       = get16(secbuf + 24);
    geo_heads     = get16(secbuf + 26);
    t16           = (unsigned long)get16(secbuf + 19);
    t32           = get32(secbuf + 32);
    total_secs    = t16 ? t16 : t32;

    if (bytes_per_sec != SECSIZE) {
        sprintf(why, "sector size is %u bytes; UNDEL handles 512 only",
                bytes_per_sec);
        return -1;
    }
    if (sec_per_clus == 0 || nfats == 0 || spf == 0 || total_secs == 0UL) {
        strcpy(why, "this does not look like a FAT volume");
        return -1;
    }
    if (root_ents == 0) {
        strcpy(why, "no fixed root directory (FAT32?) - not supported");
        return -1;
    }

    fat_area   = (unsigned long)nfats * (unsigned long)spf;
    fat_start  = (unsigned long)get16(secbuf + 14);      /* reserved       */
    root_start = fat_start + fat_area;
    /* Round the root directory up to a whole number of sectors. */
    root_secs  = ((unsigned long)root_ents * 32UL
                  + (unsigned long)bytes_per_sec - 1UL)
                 / (unsigned long)bytes_per_sec;
    data_start = root_start + root_secs;
    clus_bytes = (unsigned long)sec_per_clus * (unsigned long)bytes_per_sec;

    if (data_start >= total_secs) {
        strcpy(why, "the BPB does not add up (data area past the end)");
        return -1;
    }
    data_secs = total_secs - data_start;
    total_clus = data_secs / (unsigned long)sec_per_clus;
    /* Clusters are numbered from 2, so the last one is total_clus + 1. */
    max_clus   = total_clus + 1UL;
    if (total_clus >= 65525UL) {
        sprintf(why, "%lu clusters (FAT32) - UNDEL is FAT12/FAT16 only",
                total_clus);
        return -1;
    }
    is_fat12 = (total_clus < 4085UL);
    return 0;
}

/* Value of the FAT entry for 'clus'.  We only ever ask "is this cluster
 * still free (0)?", but the full value is returned anyway.  Two sectors
 * are cached because a FAT12 entry can straddle a sector boundary. */
static unsigned fat_entry(unsigned long clus)
{
    unsigned long off, sec;
    unsigned idx;
    unsigned v;

    if (is_fat12)
        off = clus + (clus >> 1);           /* clus * 3 / 2 */
    else
        off = clus * 2UL;
    sec = fat_start + off / (unsigned long)bytes_per_sec;
    idx = (unsigned)(off % (unsigned long)bytes_per_sec);

    if (fatbuf_lba != sec) {
        unsigned cnt = 2u;
        if (sec + 1UL >= total_secs)
            cnt = 1u;                       /* never read past the volume */
        if (cnt == 1u)
            memset(fatbuf + SECSIZE, 0, SECSIZE);
        if (disk_read(sec, cnt, fatbuf) != 0) {
            fatbuf_lba = BADLBA;
            fat_err = 1;
            return 0xFFFFu;                 /* treated as "in use"        */
        }
        fatbuf_lba = sec;
    }
    v = (unsigned)fatbuf[idx] | ((unsigned)fatbuf[idx + 1] << 8);
    if (is_fat12) {
        if (clus & 1UL)
            v >>= 4;
        else
            v &= 0x0FFFu;
    }
    return v;
}

/* --- Directory entries -------------------------------------------------- */

/* DOS name characters we refuse to put in a destination filename. */
static int bad_name_char(int c)
{
    if (c < 0x20 || c == 0x7F)
        return 1;
    return strchr("\"*+,/:;<=>?[\\]|", c) != NULL;
}

/* Rebuild "NAME.EXT" from a 32-byte directory entry, with '_' standing
 * in for the first character, which DOS destroyed with 0xE5. */
static void build_name(const unsigned char *e, char *out)
{
    int i, n = 0;
    char ext[4];
    int ne = 0;

    out[n++] = '_';                         /* the letter DOS erased      */
    for (i = 1; i < 8; i++) {
        int c = e[i];
        if (c == ' ')
            break;
        out[n++] = (char)(bad_name_char(c) ? '_' : c);
    }
    for (i = 8; i < 11; i++) {
        int c = e[i];
        if (c == ' ')
            break;
        ext[ne++] = (char)(bad_name_char(c) ? '_' : c);
    }
    ext[ne] = '\0';
    if (ne > 0) {
        out[n++] = '.';
        for (i = 0; i < ne; i++)
            out[n++] = ext[i];
    }
    out[n] = '\0';
}

/* Work out whether the data is plausibly still on the disk, and say so
 * in words.  This is the heart of the tool: it must never flatter the
 * disk.  Deletion frees the chain, so the only recoverable shape is a
 * contiguous run of clusters starting at the entry's start cluster. */
static void judge(DENT *d)
{
    unsigned long i, c;

    d->need = 0UL;
    d->taken = 0UL;

    if (d->attr & 0x10u) {
        d->verdict = V_NONE;
        strcpy(d->note, "subdirectory - not supported");
        return;
    }
    if (d->size == 0UL || d->first == 0) {
        d->verdict = V_NONE;
        strcpy(d->note, "nothing to recover (no data)");
        return;
    }
    if ((unsigned long)d->first < 2UL || (unsigned long)d->first > max_clus) {
        d->verdict = V_ERR;
        sprintf(d->note, "start cluster %u is out of range", d->first);
        return;
    }

    /* ceil(size / clusterBytes) without ever overflowing. */
    d->need = d->size / clus_bytes;
    if (d->size % clus_bytes)
        d->need++;
    if (d->need > total_clus) {
        d->verdict = V_ERR;
        sprintf(d->note, "size needs %lu clusters - impossible", d->need);
        return;
    }

    /* Walk the run of clusters the size implies and count how many have
     * been handed to some other file since the delete. */
    fat_err = 0;
    for (i = 0UL; i < d->need; i++) {
        c = (unsigned long)d->first + i;     /* LONG: 16-bit int would wrap */
        if (c > max_clus)
            break;                           /* runs off the end of the disk */
        if (fat_entry(c) != 0u)
            d->taken++;
    }

    if (fat_err) {
        d->verdict = V_DOUBT;
        strcpy(d->note, "FAT unreadable - verdict uncertain");
        return;
    }
    if (d->taken > 0UL) {
        d->verdict = V_OVER;
        sprintf(d->note, "overwritten - %lu of %lu clusters taken",
                d->taken, d->need);
        return;
    }
    if ((unsigned long)d->first + d->need - 1UL > max_clus) {
        d->verdict = V_DOUBT;
        strcpy(d->note, "runs past volume end - would truncate");
        return;
    }
    d->verdict = V_GOOD;
    strcpy(d->note, "good - contiguous and unoverwritten");
}

/* Harvest the deleted entries out of one directory sector. */
static void scan_sector(const unsigned char *sp)
{
    unsigned off;

    for (off = 0u; off + 32u <= bytes_per_sec; off += 32u) {
        const unsigned char *e = sp + off;
        if (e[0] != 0xE5u)
            continue;                   /* only deleted slots          */
        if (e[11] == 0x0Fu)
            continue;                   /* long-filename fragment      */
        if (e[11] & 0x08u)
            continue;                   /* volume label                */
        n_found++;
        if (n_del >= MAX_DEL)
            continue;                   /* list full; keep counting    */
        memset(&dents[n_del], 0, sizeof(DENT));
        build_name(e, dents[n_del].name);
        dents[n_del].attr  = e[11];
        dents[n_del].time  = get16(e + 22);
        dents[n_del].date  = get16(e + 24);
        dents[n_del].first = get16(e + 26);
        dents[n_del].size  = get32(e + 28);
        judge(&dents[n_del]);
        n_del++;
    }
}

/* Scan every 32-byte slot of the root directory for 0xE5 entries.  We do
 * NOT stop at the first 0x00 "never used" slot: it costs a few sectors to
 * read the whole fixed-size root, and deleted entries can sit beyond it.
 *
 * The root is read in 8 KB runs through iobuf rather than one sector per
 * INT 21h call - a 512-entry root is 32 sectors, so that is two DOS calls
 * instead of thirty-two, and on a diskette the difference is audible.
 *
 * A run that fails is RE-READ SECTOR BY SECTOR, and the sectors that
 * still will not come back are skipped rather than abandoning the scan.
 * This is a rescue tool pointed at disks that are already in trouble: one
 * bad sector in the root used to cost the user every entry behind it.
 * n_badsec carries the count to the list screen so the shortfall is
 * stated rather than hidden.
 *
 * Returns 0 on success, -1 when nothing at all could be read, -2 when the
 * user pressed Esc. */
static int scan_root(void)
{
    unsigned long sec;
    unsigned      chunk, s;
    int           any = 0;

    n_del = 0;
    n_found = 0;
    n_badsec = 0;
    for (sec = 0UL; sec < root_secs; sec += (unsigned long)chunk) {
        if (abort_pressed())
            return -2;

        chunk = IOSECS;
        if ((unsigned long)chunk > root_secs - sec)
            chunk = (unsigned)(root_secs - sec);

        if (disk_read(root_start + sec, chunk, iobuf) == 0) {
            for (s = 0u; s < chunk; s++)
                scan_sector(iobuf + (unsigned)s * bytes_per_sec);
            any = 1;
        } else {
            /* Narrow the damage down to the sectors that really fail. */
            for (s = 0u; s < chunk; s++) {
                if (disk_read(root_start + sec + (unsigned long)s, 1u,
                              secbuf) == 0) {
                    scan_sector(secbuf);
                    any = 1;
                } else {
                    n_badsec++;
                }
            }
        }
        ui_hbar(4, 12, 72, permille(sec + (unsigned long)chunk, root_secs),
                A_TITLE, A_HINT);
    }
    return any ? 0 : -1;
}

/* --- Destination handling ----------------------------------------------- */

static int dest_drive_index(const char *path)
{
    if (path[0] != '\0' && path[1] == ':') {
        int c = path[0];
        if (c >= 'a' && c <= 'z')
            c -= 32;
        if (c >= 'A' && c <= 'Z')
            return c - 'A';
    }
    return cur_drive();             /* no letter: the DOS default drive   */
}

/* THE refusal.  A destination on the source drive could land on exactly
 * the clusters we are about to read back. */
static int dest_ok(void)
{
    if (dest_drive_index(dest) == src_drive) {
        notify("REFUSED: that destination is on the SOURCE drive.",
               "Writing there can overwrite the very clusters that still",
               "hold your deleted files.  Choose a different drive.");
        return 0;
    }
    return 1;
}

static void join_path(char *out, const char *dir, const char *name)
{
    int n = (int)strlen(dir);
    strcpy(out, dir);
    if (n > 0 && out[n - 1] != '\\')
        strcat(out, "\\");
    strcat(out, name);
}

static int file_exists(const char *p)
{
    FILE *fp = fopen(p, "rb");      /* destination drive only             */
    if (fp == NULL)
        return 0;
    fclose(fp);
    return 1;
}

/* Losing the first letter makes collisions likely (PANEL.TXT and
 * DANEL.TXT both become _ANEL.TXT), so a clashing name gets a numeric
 * tail and the summary reports how many were renamed. */
static int unique_path(char *out, const char *base)
{
    char stem[16], ext[8], cand[20];
    const char *dot;
    int i, n;

    join_path(out, dest, base);
    if (!file_exists(out))
        return 0;

    dot = strchr(base, '.');
    n = dot ? (int)(dot - base) : (int)strlen(base);
    if (n > 6)
        n = 6;
    memcpy(stem, base, (size_t)n);
    stem[n] = '\0';
    if (dot) {
        strncpy(ext, dot, sizeof(ext) - 1);
        ext[sizeof(ext) - 1] = '\0';
    } else {
        ext[0] = '\0';
    }
    for (i = 1; i <= 99; i++) {
        sprintf(cand, "%s%02d%s", stem, i, ext);
        join_path(out, dest, cand);
        if (!file_exists(out))
            return 1;
    }
    return 1;                       /* give up uniquifying; caller reports */
}

/* --- Screens ------------------------------------------------------------ */

static void title_bar(void)
{
    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA UNDELETE", A_TITLE);
    ui_puts(21, 0, "Read-only recovery to another drive",
            UI_ATTR(C_WHITE, C_BLUE));
}

static const char *drive_note(int kind)
{
    switch (kind) {
    case 1:  return "removable";
    case 2:  return "fixed disk";
    case 3:  return "network/CD - sectors not readable";
    default: return "";
    }
}

/* Drive picker.  Returns 1 when a drive was chosen, 0 on Esc. */
static int pick_drive(void)
{
    static int list[26];
    static int kind[26];
    int n = 0, i, sel = 0, key, nfd;
    char buf[64];

    nfd = floppy_count();
    for (i = 0; i < 26 && n < 12; i++) {     /* 12 rows is all we show */
        int k;
        if (i == 0 && nfd < 1)
            continue;
        if (i == 1 && nfd < 2)
            continue;               /* phantom B: on a one-floppy machine */
        k = drive_kind(i);
        if (k == 0)
            continue;
        list[n] = i;
        kind[n] = k;
        n++;
    }
    if (n == 0) {
        title_bar();
        ui_center(10, "DOS reports no drives at all.", A_WARN);
        ui_center(12, "Press any key to leave.", A_HINT);
        wait_key();
        return 0;
    }
    for (i = 0; i < n; i++)
        if (list[i] == src_drive)
            sel = i;

    for (;;) {
        title_bar();
        ui_center(3, "Which drive holds the deleted files?", A_TITLE);
        ui_box(20, 5, 40, n + 2, A_FRAME);
        for (i = 0; i < n; i++) {
            unsigned char a = (i == sel) ? A_ITEMSEL
                              : (kind[i] == 3) ? A_HINT : A_ITEM;
            sprintf(buf, " %c:   %-30.30s", 'A' + list[i],
                    drive_note(kind[i]));
            ui_fill(21, 6 + i, 38, 1, ' ', a);
            ui_putlim(21, 6 + i, buf, 38, a);
        }
        ui_center(19,
            "UNDEL never writes to the disk it reads.  Recovered files",
            A_HINT);
        ui_center(20,
            "are copied out to a different drive that you choose.", A_HINT);
        ui_center(22,
            "Deletion frees the cluster chain, so only files stored",
            A_HINT);
        ui_center(23,
            "CONTIGUOUSLY can be rebuilt.  Nothing else can be.",
            A_HINT);
        ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
        ui_puts(2, SCR_H - 1,
                " Up/Down Select   Enter Scan   Esc Exit", A_STATUS);

        key = wait_key();
        if (key == KEY_ESC)
            return 0;
        if (key == KEY_UP)
            sel = (sel > 0) ? sel - 1 : n - 1;
        else if (key == KEY_DOWN)
            sel = (sel < n - 1) ? sel + 1 : 0;
        else if (key == KEY_ENTER) {
            if (kind[sel] == 3) {
                notify("That is a network or CD-ROM drive.",
                       "DOS cannot give us its raw sectors, so there is",
                       "nothing UNDEL can read there.");
                continue;
            }
            src_drive = list[sel];
            /* Default destination: never the source drive. */
            strcpy(dest, (src_drive == 2) ? "A:\\RESCUE" : "C:\\RESCUE");
            return 1;
        }
    }
}

/* DOS directory date word: bits 15-9 year-1980, 8-5 month, 4-0 day.
 * Time word: bits 15-11 hours, 10-5 minutes, 4-0 seconds/2. */
static void fmt_date(unsigned d, char *out)
{
    unsigned day = d & 0x1Fu, mon = (d >> 5) & 0x0Fu, yr = (d >> 9) & 0x7Fu;
    if (d == 0u)
        strcpy(out, "  --    ");
    else
        sprintf(out, "%02u/%02u/%02u", day, mon, (yr + 80u) % 100u);
}

static void fmt_time(unsigned t, char *out)
{
    sprintf(out, "%02u:%02u", (t >> 11) & 0x1Fu, (t >> 5) & 0x3Fu);
}

static unsigned char verdict_attr(int v, int selected)
{
    if (selected)
        return A_ITEMSEL;
    switch (v) {
    case V_GOOD:  return UI_ATTR(C_LGREEN, C_BLUE);
    case V_DOUBT: return UI_ATTR(C_YELLOW, C_BLUE);
    case V_OVER:
    case V_ERR:   return UI_ATTR(C_LRED, C_BLUE);
    default:      return A_HINT;
    }
}

static int recoverable(const DENT *d)
{
    return (d->verdict == V_GOOD || d->verdict == V_DOUBT);
}

static void tag_totals(int *files, unsigned long *bytes)
{
    int i;
    *files = 0;
    *bytes = 0UL;
    for (i = 0; i < n_del; i++) {
        if (dents[i].tag) {
            (*files)++;
            *bytes += dents[i].size;
        }
    }
}

#define LIST_Y  5
#define VIS     16

static void draw_list(int sel, int top)
{
    char line[96], sz[16], dt[16], tm[8];
    int row, gi, tf;
    unsigned long tb;

    title_bar();
    sprintf(line, "Source: %c:  %s  %u sec/clus (%lu B)  %lu clusters",
            'A' + src_drive, is_fat12 ? "FAT12" : "FAT16",
            sec_per_clus, clus_bytes, total_clus);
    ui_putlim(2, 2, line, 59, UI_ATTR(C_YELLOW, C_BLUE));
    if (n_badsec > 0) {
        /* Entries may be missing, and the user has to know which way the
         * list errs: short, never invented. */
        sprintf(line, "%d root sector(s) bad", n_badsec);
        ui_putlim(62, 2, line, 17, A_WARN);
    } else if (n_found > n_del) {
        /* Say plainly that the list could not hold everything. */
        sprintf(line, "%d found, %d shown", n_found, n_del);
        ui_putlim(62, 2, line, 17, A_WARN);
    } else {
        sprintf(line, "read: %s", read_mode ? "440Dh/61h" : "21h/7305h");
        ui_putlim(62, 2, line, 17, A_HINT);
    }

    tag_totals(&tf, &tb);
    sprintf(line, "Dest:   %-32.32s", dest);
    ui_puts(2, 3, line, UI_ATTR(C_YELLOW, C_BLUE));
    sprintf(line, "Tagged: %d file(s), %lu KB   ", tf, tb / 1024UL);
    ui_putlim(48, 3, line, 30, UI_ATTR(C_LGREEN, C_BLUE));

    ui_box(0, 4, SCR_W, 18, A_FRAME);
    for (row = 0; row < VIS; row++) {
        gi = top + row;
        ui_fill(1, LIST_Y + row, SCR_W - 2, 1, ' ', A_DESKTOP);
        if (gi >= n_del)
            continue;
        if (gi == sel)
            ui_fill(1, LIST_Y + row, SCR_W - 2, 1, ' ', A_ITEMSEL);
        sprintf(sz, "%lu", dents[gi].size);
        fmt_date(dents[gi].date, dt);
        sprintf(line, "%c %-12.12s %9.9s %8.8s  ",
                dents[gi].tag ? 0x10 : ' ', dents[gi].name, sz, dt);
        ui_puts(2, LIST_Y + row, line,
                (gi == sel) ? A_ITEMSEL : A_ITEM);
        ui_putlim(37, LIST_Y + row, dents[gi].note, 38,
                  verdict_attr(dents[gi].verdict, gi == sel));
    }

    if (n_del > 0) {
        fmt_time(dents[sel].time, tm);
        sprintf(line, "%s  start cluster %u   needs %lu cluster(s)   "
                "%lu still in use", tm, dents[sel].first, dents[sel].need,
                dents[sel].taken);
        ui_putlim(2, 22, line, 76, UI_ATTR(C_LCYAN, C_BLUE));
    } else {
        ui_putlim(2, 22, "No deleted entries in the root directory.", 76,
                  A_HINT);
    }
    ui_putlim(2, 23, "Names start '_': DOS erased letter 1.  "
              "Only contiguous files can be rebuilt.", 76, A_HINT);

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(0, SCR_H - 1,
        " Space Tag  * All good  F4 Dest  F5 RECOVER  Esc Back", A_STATUS);
}

/* --- Recovery ------------------------------------------------------------
 * Reads the assumed-contiguous run of clusters and writes them to the
 * destination drive.  Returns 0 ok, 1 ok-but-truncated, -1 source read
 * error, -2 destination write error, -3 aborted by the user. */
static int recover_one(const DENT *d, const char *path,
                       unsigned long *done_total, unsigned long grand,
                       unsigned long t0)
{
    FILE *fp;
    unsigned long remaining = d->size;
    unsigned long clus = (unsigned long)d->first;
    unsigned long fdone = 0UL;
    unsigned long lba;
    unsigned long avail;                /* sectors left before volume end */
    unsigned long want;                 /* sectors the tail still needs   */
    unsigned long bytes;
    unsigned n;
    long el;
    int truncated = 0;
    char buf[64];

    /* judge() never lets a bogus start cluster be tagged, but never do
     * (clus - 2) arithmetic on one anyway. */
    if (d->size == 0UL || clus < 2UL || clus > max_clus)
        return -1;

    fp = fopen(path, "wb");             /* DESTINATION drive only */
    if (fp == NULL)
        return -2;

    /* The whole point of this tool is that a recoverable file is a
     * CONTIGUOUS run of clusters, so it is also a contiguous run of
     * SECTORS: read it in 8 KB strides straight through the cluster
     * boundaries instead of restarting the arithmetic at every one.  On a
     * diskette (one sector per cluster) that is sixteen times fewer DOS
     * calls for the same bytes.
     *
     * Cluster N starts at dataStart + (N-2)*sectorsPerCluster, and the
     * run may not pass max_clus - past there the file is truncated, which
     * is reported rather than papered over. */
    lba   = data_start + (clus - 2UL) * (unsigned long)sec_per_clus;
    avail = (max_clus - clus + 1UL) * (unsigned long)sec_per_clus;

    while (remaining > 0UL) {
        if (avail == 0UL) {             /* ran off the end of the volume */
            truncated = 1;
            break;
        }
        want = (remaining + (unsigned long)bytes_per_sec - 1UL)
               / (unsigned long)bytes_per_sec;
        n = IOSECS;
        if ((unsigned long)n > avail) n = (unsigned)avail;
        if ((unsigned long)n > want)  n = (unsigned)want;

        if (disk_read(lba, n, iobuf) != 0) {
            fclose(fp);
            remove(path);
            return -1;
        }
        bytes = (unsigned long)n * (unsigned long)bytes_per_sec;
        if (bytes > remaining)
            bytes = remaining;          /* last chunk: only the real tail */
        if (fwrite(iobuf, 1, (size_t)bytes, fp) != (size_t)bytes) {
            fclose(fp);
            remove(path);
            return -2;
        }
        remaining   -= bytes;
        fdone       += bytes;
        *done_total += bytes;
        lba         += (unsigned long)n;
        avail       -= (unsigned long)n;

        ui_hbar(4, 14, 72, permille(fdone, d->size), A_TITLE, A_HINT);
        ui_hbar(4, 17, 72, permille(*done_total, grand),
                UI_ATTR(C_LGREEN, C_BLUE), A_HINT);
        el = (long)(ui_ticks() - t0);
        if (el > 0L) {
            sprintf(buf, "%lu KB   %ld KB/s   ",
                    *done_total / 1024UL,
                    (long)((*done_total >> 10) * 182L / (el * 10L)));
            ui_puts(4, 18, buf, A_PANEL);
        }
        if (abort_pressed()) {
            fclose(fp);
            remove(path);               /* no half files left behind      */
            return -3;
        }
    }
    fclose(fp);
    return truncated ? 1 : 0;
}

static void recover_panel(void)
{
    ui_fill(4, 11, 72, 9, ' ', A_PANEL);
    ui_box(3, 10, 74, 11, A_PANEL);
    ui_fill(4, 10, 72, 1, ' ', A_PANELHDR);
    ui_puts(5, 10, " Recovering (source disk is read-only) ", A_PANELHDR);
}

static void recover_tagged(void)
{
    int i, tf, done = 0, trunc = 0, errors = 0, renamed = 0;
    unsigned long tb, done_total = 0UL, t0;
    /* The closing summary formats five numbers into one line; at 80 bytes
     * it had about three to spare against a wide byte count and a fast
     * drive.  The display is clipped to 70 columns either way, and a
     * smashed stack in the middle of a recovery is the worst moment for
     * one, so the buffer is sized for the widest the format can produce. */
    char path[128], buf[128];
    long el;

    tag_totals(&tf, &tb);
    if (tf == 0) {
        notify("Nothing is tagged.",
               "Tag recoverable files with Space, or press * to tag",
               "every file whose verdict is good.");
        return;
    }
    if (!dest_ok())
        return;

    /* Prove we can write to the destination before touching anything. */
    join_path(path, dest, "UNDEL$$.TMP");
    {
        FILE *fp = fopen(path, "wb");
        if (fp == NULL) {
            dos_mkdir(dest);
            fp = fopen(path, "wb");
        }
        if (fp == NULL) {
            notify("Cannot write to the destination:", dest,
                   "Check the drive letter, the disk and the write tab.");
            return;
        }
        fclose(fp);
        remove(path);
    }

    recover_panel();
    t0 = ui_ticks();
    for (i = 0; i < n_del; i++) {
        int rc;
        if (!dents[i].tag)
            continue;

        if (unique_path(path, dents[i].name))
            renamed++;
        sprintf(buf, "%-12.12s %lu bytes  ->  %-24.24s",
                dents[i].name, dents[i].size, path);
        ui_putlim(5, 12, buf, 70, A_PANEL);
        ui_puts(5, 13, "file: ", A_PANEL);
        ui_puts(5, 16, "total:", A_PANEL);

        rc = recover_one(&dents[i], path, &done_total, tb, t0);
        if (rc == 0 || rc == 1) {
            done++;
            if (rc == 1)
                trunc++;
            dents[i].tag = 0;
        } else if (rc == -3) {
            ui_putlim(5, 19, "Aborted - the partial file was removed.", 70,
                      A_WARN);
            break;
        } else {
            errors++;
            sprintf(buf, "ERROR on %s: %s", dents[i].name,
                    (rc == -1) ? "source read failed (bad sectors?)"
                               : "destination write failed (disk full?)");
            ui_putlim(5, 19, buf, 70, A_WARN);
            ui_puts(5, 18, "Press a key to continue with the rest...",
                    A_PANEL);
            wait_key();
            ui_fill(5, 18, 70, 2, ' ', A_PANEL);
        }
    }

    el = (long)(ui_ticks() - t0);
    if (el < 1L)
        el = 1L;
    ui_fill(4, 12, 72, 8, ' ', A_PANEL);        /* clear bars and lines */
    sprintf(buf, "%d recovered, %lu KB, %d truncated, %d error(s), %ld KB/s",
            done, done_total / 1024UL, trunc, errors,
            (long)((done_total >> 10) * 182L / (el * 10L)));
    ui_putlim(5, 13, buf, 70, errors ? A_WARN : A_PANEL);
    ui_putlim(5, 15, "Letter 1 of every name is '_': DOS overwrote the "
              "original with 0xE5.", 70, A_PANEL);
    if (renamed > 0) {
        sprintf(buf, "%d name(s) got a numeric tail: losing letter 1 "
                "causes clashes.", renamed);
        ui_putlim(5, 16, buf, 70, A_PANEL);
    }
    if (trunc > 0)
        ui_putlim(5, 17, "Truncated file(s): the run of clusters ended at "
                  "the end of the volume.", 70, A_WARN);
    ui_puts(5, 19, " Press any key to return. ", A_PANELHDR);
    wait_key();
}

/* --- Main list loop ------------------------------------------------------ */

static void browse_list(void)
{
    int sel = 0, top = 0, key, i;

    for (;;) {
        if (sel < 0)
            sel = 0;
        if (sel >= n_del)
            sel = n_del ? n_del - 1 : 0;
        if (sel < top)
            top = sel;
        if (sel >= top + VIS)
            top = sel - VIS + 1;
        draw_list(sel, top);
        key = wait_key();

        if (key == KEY_ESC || key == KEY_F10) {
            return;
        } else if (key == KEY_UP) {
            if (sel > 0) sel--;
        } else if (key == KEY_DOWN) {
            if (sel < n_del - 1) sel++;
        } else if (key == KEY_PGUP) {
            sel -= VIS;
        } else if (key == KEY_PGDN) {
            sel += VIS;
        } else if (key == KEY_HOME) {
            sel = 0;
        } else if (key == KEY_END) {
            sel = n_del - 1;
        } else if (key == KEY_SPACE || key == KEY_INS) {
            if (n_del > 0) {
                if (recoverable(&dents[sel])) {
                    dents[sel].tag = !dents[sel].tag;
                    if (sel < n_del - 1) sel++;
                } else if (dents[sel].verdict == V_OVER) {
                    notify("Those clusters belong to another file now.",
                           "Copying them out would hand you somebody else's",
                           "data, not yours, so UNDEL will not do it.");
                } else {
                    notify("There is nothing to recover for that entry.",
                           dents[sel].note, NULL);
                }
            }
        } else if (key == '*') {
            for (i = 0; i < n_del; i++)
                if (dents[i].verdict == V_GOOD)
                    dents[i].tag = 1;
        } else if (key == '-') {
            for (i = 0; i < n_del; i++)
                dents[i].tag = 0;
        } else if (key == KEY_F4) {
            int w = 62, h = 8, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
            ui_fill(x, y, w, h, ' ', A_PANEL);
            ui_box(x, y, w, h, A_PANEL);
            ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
            ui_puts(x + 2, y, " Destination path (a DIFFERENT drive) ",
                    A_PANELHDR);
            ui_putlim(x + 3, y + 2,
                      "It is created if it does not exist.", w - 6, A_PANEL);
            if (ui_editline(dest, (int)sizeof(dest), x + 3, y + 4, w - 6))
                dest_ok();
        } else if (key == KEY_F5) {
            recover_tagged();
        }
    }
}

/* --- Entry point --------------------------------------------------------- */

int main(void)
{
    char why[80], msg[96];
    int rc;

    ui_init();
    src_drive = 0;

    for (;;) {
        if (!pick_drive())
            break;                      /* Esc: leave (ui_done below)     */

        title_bar();
        sprintf(msg, "Reading drive %c: ...", 'A' + src_drive);
        ui_center(8, msg, A_TITLE);
        ui_center(16, "Reading only.  Nothing is written to this disk.",
                  A_HINT);

        rc = disk_open();
        if (rc != 0) {
            sprintf(msg, "Cannot read the boot sector (DOS error %d).", rc);
            notify(msg,
                   "The drive may be empty, unformatted or unreadable.",
                   "No sector of it has been changed in any way.");
            continue;
        }
        if (parse_bpb(why) != 0) {
            notify("This volume cannot be scanned:", why,
                   "UNDEL handles FAT12/FAT16 with 512-byte sectors.");
            continue;
        }

        sprintf(msg, "Scanning the root directory (%u entries)...",
                root_ents);
        ui_center(10, msg, A_HINT);
        ui_puts(4, 14, "Esc cancels.", A_HINT);
        rc = scan_root();
        if (rc == -1) {
            notify("The root directory could not be read.",
                   "The disk may be failing; try CASTALIA DISK DOCTOR.",
                   "Nothing was written to it either way.");
            continue;
        }
        if (rc == -2)
            continue;                   /* user cancelled the scan        */

        browse_list();                  /* Esc there returns to the picker */
    }

    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();                          /* the only way out of this file  */
    return 0;
}
