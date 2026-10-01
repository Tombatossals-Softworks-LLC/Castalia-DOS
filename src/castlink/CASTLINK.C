/* ===================================================================
 * CASTLINK.C  -  CASTALIA LINK: null-modem serial file transfer
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * A LapLink-style file mover for two machines joined by a null-modem
 * (crossover) cable.  Run CASTLINK on both keeps, pick the same COM
 * speed, set one to Receive and the other to Send, tag files and go.
 * Live per-file and total bars, KB/s, elapsed time, error counters.
 *
 * DIRECT UART I/O, NOT BIOS INT 14h: INT 14h re-enters the BIOS for
 * every single byte, caps at 9600 baud on most PC/AT BIOSes and hides
 * the 16550 FIFO, so this talks to the 8250/16550 registers directly.
 * It stays 100% POLLED - IER is 0, no vector is ever hooked and no TSR
 * is installed (a hard project rule).  The only cost of polling is
 * that the loops must be bounded, and they are:
 *
 * NO HANG, EVER: every byte read and every byte write is wrapped in a
 * tick-based timeout (ui_ticks(), 18.2 Hz), calls ui_idle() while it
 * waits, and polls ui_keywaiting() so Esc aborts at any moment.  A
 * missing cable, a powered-off partner or a mismatched speed produces
 * a clear message, never a locked machine.
 *
 * WIRE PROTOCOL  "CASTLINK/1"  -  stop-and-wait, CRC-16 checked
 *
 *   +------+------+---------+------------------+---------+
 *   | SOH  | TYPE | LEN_LO  |   payload[LEN]   | CRC_LO  |
 *   | 0x01 |  1 b | LEN_HI  |   0..1024 bytes  | CRC_HI  |
 *   +------+------+---------+------------------+---------+
 *      0      1      2   3    4 .. 4+LEN-1      +LEN +LEN+1
 *
 *   Lengths and CRC are little-endian.  The CRC is CRC-16/CCITT (poly
 *   0x1021, init 0xFFFF) over the payload only.  The receiver answers
 *   every frame with one raw byte, ACK (0x06) or NAK (0x15); the
 *   sender retries a frame MAX_RETRY times then aborts with a clear
 *   message.  The receiver resynchronises by dropping bytes until it
 *   sees SOH.  Frame types:
 *
 *   'H' HELLO  0-7 "CASTLINK", 8 protocol version, 9 role 'S'/'R',
 *              10-11 file count LE16, 12-15 grand total bytes LE32.
 *              The sender greets first; the receiver ACKs and greets
 *              back (counts zero), so both ends check the version and
 *              the receiver can size its total progress bar.
 *   'F' FILE   0-3 file size LE32, 4.. name, NUL-terminated.
 *   'D' DATA   up to 1024 raw file bytes, in order.
 *   'E' EOF    0-3 bytes sent LE32, 4-5 CRC-16 of the whole file, so
 *              the receiver verifies instead of keeping a corrupt copy.
 *              A file that fails that check, could not be written in
 *              full, or never got its EOF (link lost, Esc, a new FILE
 *              header) is deleted: only verified files stay on disk.
 *              The sender withholds the EOF of a file it could not read
 *              to the end, so a short read is never verified as whole.
 *              An existing file is never overwritten - the new one is
 *              saved as NAME~n.EXT, or refused once ~1..~99 are taken.
 *   'Z' DONE   end of the job.   'X' ABORT  the far end cancelled.
 *
 * There is deliberately no autobaud: the user picks the same speed on
 * both machines from the setup screen before the cable carries data.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os castlink.c ..\common\ui.c ..\common\dirw.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include "../common/UI.H"
#include "../common/DIRW.H"

#if defined(__WATCOMC__)
#include <conio.h>              /* inp() / outp() - real hardware build */
#endif

/* --- Test seam: CASTLINK_TEST ----------------------------------------
 * This wire protocol can never be exercised on the real thing here: it
 * wants two machines and a null-modem cable, so nothing in CI can plug
 * one in.  Its correctness therefore has to be provable without a UART,
 * and that needs exactly two things - a way to feed bytes to pin() and
 * catch the bytes pout() writes, and a way to keep main() out of the
 * way.  tests/unit/test_castlink.c compiles THIS file with
 * -DCASTLINK_TEST, supplies the two hooks below as a virtual UART and
 * supplies its own main(), which lets it drive send_frame()/recv_frame()
 * end to end.  No DOS build ever defines CASTLINK_TEST, and Open Watcom
 * takes the __WATCOMC__ branches regardless, so the shipped program is
 * byte-for-byte what it was without this seam.
 */
#ifdef CASTLINK_TEST
extern unsigned char castlink_port_in(unsigned port);
extern void          castlink_port_out(unsigned port, unsigned char v);
#endif

/* --- Port I/O --------------------------------------------------------
 * Open Watcom supplies inp()/outp(); Turbo C spells them inportb()/
 * outportb() in <dos.h>.  The gcc syntax-check gate has no ports at
 * all, so it compiles harmless stubs - the same conditional pattern
 * CPUDET.C uses for its compiler-specific probes. */

static unsigned char pin(unsigned port)
{
#if defined(__WATCOMC__)
    return (unsigned char)inp(port);
#elif defined(__TURBOC__)
    return (unsigned char)inportb((int)port);
#elif defined(CASTLINK_TEST)
    return castlink_port_in(port);      /* virtual UART - see the seam */
#else
    (void)port;  return 0;              /* host syntax-check only */
#endif
}

static void pout(unsigned port, unsigned char v)
{
#if defined(__WATCOMC__)
    outp(port, v);
#elif defined(__TURBOC__)
    outportb((int)port, v);
#elif defined(CASTLINK_TEST)
    castlink_port_out(port, v);         /* virtual UART - see the seam */
#else
    (void)port;  (void)v;               /* host syntax-check only */
#endif
}

/* --- 8250/16550 register map (offsets from the port base) ----------- */
#define U_RBR   0       /* R: receive buffer   W: transmit holding     */
#define U_DLL   0       /* divisor low   (only while LCR bit 7, DLAB)  */
#define U_IER   1       /* interrupt enable          (DLM while DLAB)  */
#define U_DLM   1
#define U_FCR   2       /* W: FIFO control (16550+)  R: IIR            */
#define U_LCR   3       /* line control: word length, parity, DLAB     */
#define U_MCR   4       /* modem control: DTR, RTS, OUT1, OUT2, loop   */
#define U_LSR   5       /* line status: data ready, errors, THR empty  */
#define U_MSR   6       /* modem status: CTS, DSR, RI, DCD             */

#define LSR_DR   0x01   /* bit 0: a received byte waits in RBR         */
#define LSR_ERR  0x0E   /* bits 1-3: overrun / parity / framing        */
#define LSR_THRE 0x20   /* bit 5: transmit holding register is empty   */

/* --- Protocol -------------------------------------------------------- */
#define SOH  0x01
#define ACK  0x06   /* ACK|seq: 06h acknowledges seq 0, 07h acknowledges 1 */
#define NAK  0x15
/* Stop-and-wait needs a sequence number or a lost ACK makes the sender
 * retransmit a frame the receiver already took, and the payload lands
 * twice.  One alternating bit is enough, and the type byte has room for
 * it: every F_* code is ASCII, so bit 7 is free. */
#define SEQ_BIT 0x80
#define F_HELLO 'H'
#define F_FILE  'F'
#define F_DATA  'D'
#define F_EOF   'E'
#define F_DONE  'Z'
#define F_ABORT 'X'
#define PROTO_VER 1
#define MAXPAY    1024
#define HDRLEN    4
#define FRMMAX    (HDRLEN + MAXPAY + 2)
#define MAX_RETRY 5

/* Timeouts in BIOS ticks (18.2 per second). */
#define TMO_BYTE   36UL         /* ~2 s for the next byte of a frame   */
#define TMO_ACK    55UL         /* ~3 s for the ACK/NAK of a frame     */
#define TMO_IDLE   273UL        /* ~15 s for the next frame of the job */
#define TMO_SLICE  36UL         /* ~2 s poll slice while idle-waiting  */
#define TICKS_DAY  0x1800B0UL   /* the BIOS tick counter rolls here    */

/* Byte helpers return SER_OK or a negative code; recv_frame() returns a
 * frame type (positive) or one of the same negative codes. */
#define SER_OK       0
#define SER_TIMEOUT (-1)
#define SER_ABORT   (-2)        /* the user pressed Esc                */
#define SER_BAD     (-3)        /* bad length/CRC - NAK already sent   */
#define SER_FAIL    (-4)        /* out of retries                      */

/* --- State ------------------------------------------------------------ */
#define MAX_ENT 256
#define NAMELEN 13
#define LOGN    6

typedef struct { char name[NAMELEN]; unsigned long size; int tag; } FENT;

static FENT ents[MAX_ENT];
static int  nent = 0;
static char curdir[72];

static unsigned com_bases[4];           /* BDA COM1..COM4 base ports   */
static int      com_index = 0;          /* 0..3                        */
static int      speed_index = 3;
static int      role = 0;               /* 0 = send, 1 = receive       */
static unsigned uart = 0;               /* base port currently open    */
static const unsigned long speeds[4] = { 19200UL, 38400UL, 57600UL,
                                         115200UL };
static const int divisors[4] = { 6, 3, 2, 1 };      /* 115200 / speed  */

static unsigned char frm[FRMMAX];       /* the one and only frame buffer */

static unsigned long tot_bytes = 0, tot_done = 0, t_start = 0;
static int tot_files = 0, files_done = 0;
static int n_retry = 0;                 /* frames re-sent              */
static int n_crc = 0;                   /* frames rejected on CRC      */
static int n_line = 0;                  /* UART overrun/parity/framing */
static int n_bad = 0;                   /* files that failed to verify */
static int n_renamed = 0;
static int tx_seq = 0;                  /* alternating bit we send     */
static int rx_seq = 0;                  /* alternating bit we expect   */

static char logln[LOGN][72];
static int  log_used = 0, log_dirty = 1;

/* --- Small helpers ---------------------------------------------------- */

/* Ticks since t0, tolerating the one midnight rollover of the BIOS
 * counter (which would otherwise look like a huge interval). */
static unsigned long ticks_since(unsigned long t0)
{
    unsigned long now = ui_ticks();
    return (now >= t0) ? (now - t0) : ((TICKS_DAY - t0) + now);
}

static int esc_pressed(void)
{
    if (!ui_keywaiting()) return 0;
    return (ui_getkey() == KEY_ESC) ? 1 : 0;
}

static void put_u16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)(v & 0xFFu);
    p[1] = (unsigned char)((v >> 8) & 0xFFu);
}

static unsigned get_u16(const unsigned char *p)
{
    return (unsigned)p[0] | (unsigned)((unsigned)p[1] << 8);
}

static void put_u32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v & 0xFFUL);
    p[1] = (unsigned char)((v >> 8) & 0xFFUL);
    p[2] = (unsigned char)((v >> 16) & 0xFFUL);
    p[3] = (unsigned char)((v >> 24) & 0xFFUL);
}

static unsigned long get_u32(const unsigned char *p)
{
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8)
         | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

/* CRC-16/CCITT, bitwise so no 512-byte table is needed.  Every step is
 * masked to 16 bits on purpose: 'unsigned' is 16-bit under Watcom but
 * 32-bit on the syntax-check host, and both must yield the same CRC. */
static unsigned crc16_upd(unsigned crc, const unsigned char *p, int n)
{
    int i, b;
    for (i = 0; i < n; i++) {
        crc ^= (unsigned)(((unsigned)p[i] << 8) & 0xFFFFu);
        for (b = 0; b < 8; b++)
            crc = (crc & 0x8000u)
                ? (unsigned)(((crc << 1) & 0xFFFFu) ^ 0x1021u)
                : (unsigned)((crc << 1) & 0xFFFFu);
    }
    return crc & 0xFFFFu;
}

/* Scrolling six-line activity log shown under the progress bars. */
static void log_add(const char *s)
{
    int i;
    if (log_used >= LOGN) {
        for (i = 1; i < LOGN; i++)
            strcpy(logln[i - 1], logln[i]);
        log_used = LOGN - 1;
    }
    strncpy(logln[log_used], s, 70);
    logln[log_used][70] = '\0';
    log_used++;
    log_dirty = 1;
}

/* --- UART -------------------------------------------------------------- */

/* COM1..COM4 base addresses live in the BIOS Data Area at 0040:0000,
 * 0002, 0004 and 0006.  A zero word means no UART was found there. */
static void scan_com(void)
{
    unsigned short far *bda = (unsigned short far *)MK_FP(0x0040, 0x0000);
    int i;
    for (i = 0; i < 4; i++)
        com_bases[i] = (unsigned)bda[i];
}

/* Throw away stale bytes (a previous run, or noise from plugging in). */
static void uart_drain(void)
{
    int guard;
    for (guard = 0; guard < 512; guard++) {
        if (!(pin(uart + U_LSR) & LSR_DR)) break;
        (void)pin(uart + U_RBR);
    }
}

/* Program the UART for 8 data bits, no parity, 1 stop bit, polled. */
static void uart_open(void)
{
    int div = divisors[speed_index];
    uart = com_bases[com_index];
    pout(uart + U_IER, 0x00);       /* no interrupts - polled only     */
    pout(uart + U_LCR, 0x80);       /* DLAB=1: divisor latch visible   */
    pout(uart + U_DLL, (unsigned char)(div & 0xFF));
    pout(uart + U_DLM, (unsigned char)((div >> 8) & 0xFF));
    pout(uart + U_LCR, 0x03);       /* 8N1, DLAB back to 0             */
    pout(uart + U_FCR, 0xC7);       /* enable + clear both FIFOs, trigger
                                     * at 14 bytes.  On a plain 8250 this
                                     * lands on a read-only IIR and is
                                     * simply ignored - harmless.      */
    pout(uart + U_MCR, 0x03);       /* DTR + RTS.  OUT2 is deliberately
                                     * left clear: it gates the IRQ line
                                     * and we never want an interrupt. */
    (void)pin(uart + U_LSR);        /* clear latched error bits        */
    (void)pin(uart + U_MSR);
    uart_drain();
}

static void uart_close(void)
{
    if (uart == 0) return;
    pout(uart + U_IER, 0x00);
    pout(uart + U_MCR, 0x00);       /* drop DTR/RTS, leave it quiet    */
    uart = 0;
}

/* Send one byte: poll LSR bit 5 (THR empty), then write THR.  The Esc /
 * idle / timeout housekeeping runs once every SPIN polls so a 115200
 * transfer is not throttled by an INT 28h per byte, while the loop
 * still cannot outlive its timeout. */
#define SPIN 200
static int ser_putb(int b, unsigned long tmo)
{
    unsigned long t0 = ui_ticks();
    int spin = 0;
    for (;;) {
        if (pin(uart + U_LSR) & LSR_THRE) {
            pout(uart + U_RBR, (unsigned char)b);
            return SER_OK;
        }
        if (++spin >= SPIN) {
            spin = 0;
            if (esc_pressed()) return SER_ABORT;
            ui_idle();
            if (ticks_since(t0) > tmo) return SER_TIMEOUT;
        }
    }
}

/* Receive one byte: poll LSR bit 0 (data ready), then read RBR.  Line
 * errors (bits 1-3) are counted and the damaged byte dropped - the
 * frame CRC catches the hole and the sender retransmits. */
static int ser_getb(int *b, unsigned long tmo)
{
    unsigned long t0 = ui_ticks();
    unsigned char ls;
    int spin = 0;
    for (;;) {
        ls = pin(uart + U_LSR);
        if (ls & LSR_ERR) {
            if (n_line < 32000) n_line++;
            if (ls & LSR_DR) (void)pin(uart + U_RBR);  /* read clears */
        } else if (ls & LSR_DR) {
            *b = (int)pin(uart + U_RBR);
            return SER_OK;
        }
        if (++spin >= SPIN) {
            spin = 0;
            if (esc_pressed()) return SER_ABORT;
            ui_idle();
            if (ticks_since(t0) > tmo) return SER_TIMEOUT;
        }
    }
}

/* --- Frame layer -------------------------------------------------------- */

/* Transmit frm[HDRLEN .. HDRLEN+len-1] as a frame of the given type and
 * wait for the peer's ACK, resending the whole frame on NAK/timeout. */
static int send_frame(int type, int len)
{
    int attempt, i, rc, b, total = HDRLEN + len + 2;

    frm[0] = SOH;
    frm[1] = (unsigned char)(type | (tx_seq ? SEQ_BIT : 0));
    frm[2] = (unsigned char)(len & 0xFF);
    frm[3] = (unsigned char)((len >> 8) & 0xFF);
    /* The CRC covers the type and length bytes as well as the payload.
     * Over payload alone, a bit error that turned a DATA type into EOF
     * would still check out, and the receiver would close a half-written
     * file believing it was complete. */
    put_u16(frm + HDRLEN + len, crc16_upd(0xFFFFu, frm + 1, 3 + len));

    for (attempt = 0; attempt < MAX_RETRY; attempt++) {
        for (i = 0; i < total; i++) {
            rc = ser_putb((int)frm[i], TMO_BYTE);
            if (rc != SER_OK) return rc;        /* Esc or a dead line  */
        }
        rc = ser_getb(&b, TMO_ACK);
        if (rc == SER_ABORT) return rc;
        if (rc == SER_OK && b == (ACK | tx_seq)) {
            tx_seq ^= 1;                        /* this frame is done  */
            return SER_OK;
        }
        if (n_retry < 32000) n_retry++;
        if (rc == SER_OK && b == NAK && n_crc < 32000) n_crc++;
    }
    return SER_FAIL;
}

/* Read one frame into frm[].  Returns the frame type (positive) with
 * *plen set, or a negative SER_* code.  ACK/NAK is sent from here. */
static int recv_frame(int *plen, unsigned long tmo)
{
    int rc, b, i, type, len, seq;

    for (;;) {                              /* one pass per new frame  */
        for (;;) {                          /* resync on SOH           */
            rc = ser_getb(&b, tmo);
            if (rc != SER_OK) return rc;
            if (b == SOH) break;
        }
        /* Keep the header in frm[] so the CRC can cover it. */
        rc = ser_getb(&b, TMO_BYTE);  if (rc != SER_OK) return rc;
        frm[1] = (unsigned char)b;
        rc = ser_getb(&b, TMO_BYTE);  if (rc != SER_OK) return rc;
        frm[2] = (unsigned char)b;
        len = b;
        rc = ser_getb(&b, TMO_BYTE);  if (rc != SER_OK) return rc;
        frm[3] = (unsigned char)b;
        len |= b << 8;
        if (len < 0 || len > MAXPAY) {      /* impossible: line noise  */
            if (n_crc < 32000) n_crc++;
            (void)ser_putb(NAK, TMO_BYTE);
            return SER_BAD;
        }
        for (i = 0; i < len + 2; i++) {     /* payload, then the CRC   */
            rc = ser_getb(&b, TMO_BYTE);
            if (rc != SER_OK) return rc;
            frm[HDRLEN + i] = (unsigned char)b;
        }
        if (crc16_upd(0xFFFFu, frm + 1, 3 + len) !=
            get_u16(frm + HDRLEN + len)) {
            if (n_crc < 32000) n_crc++;
            (void)ser_putb(NAK, TMO_BYTE);
            return SER_BAD;
        }

        seq  = (frm[1] & SEQ_BIT) ? 1 : 0;
        type = (int)(frm[1] & 0x7F);

        /* Acknowledge what actually arrived, sequence and all. */
        rc = ser_putb(ACK | seq, TMO_BYTE);
        if (rc != SER_OK) return rc;

        if (seq != rx_seq) {
            /* The sender never heard our ACK and sent this frame again.
             * It is now acknowledged a second time, but handing it up
             * would append the same payload twice - which is exactly the
             * corruption the retry was meant to avoid.  Drop it and wait
             * for the next one.  (Counted as a retry: that is what it is
             * from the far end's point of view.) */
            if (n_retry < 32000) n_retry++;
            continue;
        }
        rx_seq ^= 1;
        *plen = len;
        return type;
    }
}

/* --- Presentation -------------------------------------------------------- */

static void notify(const char *l1, const char *l2)
{
    int w = 62, h = 7, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " CASTALIA LINK ", A_PANELHDR);
    if (l1) ui_putlim(x + 3, y + 2, l1, w - 6, A_PANEL);
    if (l2) ui_putlim(x + 3, y + 3, l2, w - 6, A_PANEL);
    ui_puts(x + 3, y + 5, "Press any key.", A_PANEL);
    ui_getkey();
}

/* Progress in permille, scaled down first so done*1000 stays inside 32
 * bits even for a 4 GB job. */
static int permille(unsigned long done, unsigned long total)
{
    if (total == 0UL) return 1000;
    while (total > 4000000UL) { total >>= 4; done >>= 4; }
    if (done > total) done = total;
    return (int)((done * 1000UL) / total);
}

static unsigned long kbps(unsigned long bytes, unsigned long ticks)
{
    if (ticks < 1UL) ticks = 1UL;       /* 18.2 ticks/s: *182 / (t*10) */
    return (bytes >> 10) * 182UL / (ticks * 10UL);
}

static void fmt_elapsed(char *out, unsigned long ticks)
{
    unsigned long secs = ticks * 10UL / 182UL;
    sprintf(out, "%lu:%02lu", secs / 60UL, secs % 60UL);
}

/* The static furniture of the transfer screen. */
static void xfer_screen(const char *what)
{
    char line[80];
    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA LINK", A_TITLE);
    ui_puts(18, 0, "Serial File Transfer", UI_ATTR(C_WHITE, C_BLUE));
    sprintf(line, "COM%d (%04X)  %lu bps 8N1   %s",
            com_index + 1, com_bases[com_index], speeds[speed_index], what);
    ui_puts(2, 2, line, UI_ATTR(C_YELLOW, C_BLUE));
    ui_box(0, 3, SCR_W, 19, A_FRAME);
    ui_puts(3, 5, "File:", A_HINT);
    ui_puts(3, 8, "Total:", A_HINT);
    ui_hline(1, 14, SCR_W - 2, A_FRAME);
    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(0, SCR_H - 1, " Esc  Abort the transfer and return", A_STATUS);
    log_dirty = 1;
}

/* Live display: per-file bar, total bar, rate, elapsed, counters, log. */
static void xfer_update(const char *name, unsigned long fdone,
                        unsigned long fsize)
{
    char line[80], el[16];
    unsigned long ticks = ticks_since(t_start);
    int i, shown = files_done + 1;

    if (shown > tot_files) shown = tot_files;
    sprintf(line, "%-13.13s  %10lu of %-10lu bytes", name, fdone, fsize);
    ui_puts(10, 5, line, UI_ATTR(C_WHITE, C_BLUE));
    ui_hbar(3, 6, 74, permille(fdone, fsize), A_TITLE, A_HINT);

    sprintf(line, "file %d of %-4d  %10lu of %-10lu bytes",
            shown, tot_files, tot_done, tot_bytes);
    ui_puts(10, 8, line, UI_ATTR(C_WHITE, C_BLUE));
    ui_hbar(3, 9, 74, permille(tot_done, tot_bytes),
            UI_ATTR(C_LGREEN, C_BLUE), A_HINT);

    fmt_elapsed(el, ticks);
    sprintf(line, "%lu KB moved     %lu KB/s     elapsed %s     ",
            tot_done / 1024UL, kbps(tot_done, ticks), el);
    ui_puts(3, 11, line, UI_ATTR(C_LCYAN, C_BLUE));
    sprintf(line, "retries %-5d CRC errors %-5d line errors %-5d "
            "bad files %-5d", n_retry, n_crc, n_line, n_bad);
    ui_puts(3, 12, line, (n_retry || n_crc || n_line || n_bad)
            ? UI_ATTR(C_YELLOW, C_BLUE) : A_HINT);

    if (log_dirty) {
        for (i = 0; i < LOGN; i++) {
            ui_fill(3, 15 + i, 74, 1, ' ', A_DESKTOP);
            if (i < log_used) ui_putlim(3, 15 + i, logln[i], 74, A_ITEM);
        }
        log_dirty = 0;
    }
}

static void summary(const char *headline)
{
    int w = 62, h = 12, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
    unsigned long ticks = ticks_since(t_start);
    char line[80], el[16];

    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " Transfer summary ", A_PANELHDR);
    ui_putlim(x + 3, y + 2, headline, w - 6, A_PANEL);
    fmt_elapsed(el, ticks);
    sprintf(line, "Files  : %d of %d", files_done, tot_files);
    ui_puts(x + 3, y + 4, line, A_PANEL);
    sprintf(line, "Bytes  : %lu (%lu KB)", tot_done, tot_done / 1024UL);
    ui_puts(x + 3, y + 5, line, A_PANEL);
    sprintf(line, "Time   : %s   average %lu KB/s", el, kbps(tot_done, ticks));
    ui_puts(x + 3, y + 6, line, A_PANEL);
    sprintf(line, "Retries: %d   CRC %d   line %d   renamed %d",
            n_retry, n_crc, n_line, n_renamed);
    ui_puts(x + 3, y + 7, line, A_PANEL);
    if (n_bad > 0) {
        sprintf(line, "%d file(s) FAILED - see the transfer log.", n_bad);
        ui_putlim(x + 3, y + 8, line, w - 6, A_WARN);
    }
    ui_puts(x + 3, y + 10, "Press any key to return.", A_PANEL);
    ui_getkey();
}

/* --- Local files --------------------------------------------------------- */

static int file_exists(const char *p)
{
    FILE *fp = fopen(p, "rb");
    if (fp == NULL) return 0;
    fclose(fp);
    return 1;
}

/* Current drive + directory.  INT 21h AH=47h returns the path (no
 * drive letter, no leading backslash) at DS:SI. */
static void get_cwd(char *out)
{
    static char buf[68];
    union REGS r;
    struct SREGS s;
    int drive;

    r.h.ah = 0x19;                      /* current drive, 0 = A:       */
    int86(0x21, &r, &r);
    drive = r.h.al + 'A';
    buf[0] = '\0';
    segread(&s);
    s.ds   = FP_SEG((void far *)buf);
    r.x.si = FP_OFF((void far *)buf);
    r.h.ah = 0x47;
    r.h.dl = 0;                         /* 0 = default drive           */
    int86x(0x21, &r, &r, &s);
    if (r.x.cflag) buf[0] = '\0';
    sprintf(out, "%c:\\%.60s", (char)drive, buf);
}

/* Incoming names are untrusted: strip any path the far end may have
 * embedded, and fold anything DOS would choke on into '_'. */
static void sanitize_name(char *n)
{
    char clean[NAMELEN + 1], c;
    const char *src = n, *p;
    int i, j = 0;

    for (p = n; *p != '\0'; p++)
        if (*p == '\\' || *p == '/' || *p == ':') src = p + 1;
    for (i = 0; src[i] != '\0' && j < NAMELEN - 1; i++) {
        c = src[i];
        if (c < 33 || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|' || c == ',' || c == ';') c = '_';
        clean[j++] = c;
    }
    clean[j] = '\0';
    if (j == 0) strcpy(clean, "NONAME.DAT");
    strcpy(n, clean);
}

/* Never clobber an existing file: fold NAME.EXT into NAME~1.EXT and so
 * on.  'name' needs room for 14 bytes.  Returns 0 if the name was free,
 * 1 if it was renamed, -1 if ~1..~99 are all taken too. */
static int unique_name(char *name)
{
    char base[NAMELEN + 4], ext[8], cand[24], *dot;
    int n, bl;

    if (!file_exists(name)) return 0;
    strncpy(base, name, sizeof(base) - 1);
    base[sizeof(base) - 1] = '\0';
    dot = strchr(base, '.');
    if (dot != NULL) {
        strncpy(ext, dot, sizeof(ext) - 1);
        ext[sizeof(ext) - 1] = '\0';
        if ((int)strlen(ext) > 4) ext[4] = '\0';    /* clamp to ".EXT" */
        *dot = '\0';
    } else {
        ext[0] = '\0';
    }
    for (n = 1; n <= 99; n++) {
        bl = (int)strlen(base);
        if (bl > (n < 10 ? 6 : 5)) bl = (n < 10 ? 6 : 5);
        sprintf(cand, "%.*s~%d%s", bl, base, n, ext);
        if (!file_exists(cand)) { strcpy(name, cand); return 1; }
    }
    /* Out of names.  Handing back the original would overwrite a file
     * the user already had, which is the one thing this must not do. */
    return -1;
}

/* --- Setup screen --------------------------------------------------------- */

/* Returns 1 to start the link, 0 if the user pressed Esc. */
static int setup_screen(void)
{
    char line[80];
    int field = 0, key, i, avail = 0;

    for (i = 0; i < 4; i++)
        if (com_bases[i] != 0) avail++;
    if (avail == 0) {
        notify("The BIOS reports no serial ports (COM1..COM4 all read 0).",
               "CASTLINK needs a UART and a null-modem cable.");
        return 0;
    }
    while (com_bases[com_index] == 0)
        com_index = (com_index + 1) & 3;

    for (;;) {
        ui_cls(A_DESKTOP);
        ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
        ui_puts(2, 0, "CASTALIA LINK", A_TITLE);
        ui_puts(18, 0, "Serial File Transfer", UI_ATTR(C_WHITE, C_BLUE));

        ui_puts(4, 2, "Detected UARTs:", A_HINT);
        for (i = 0; i < 4; i++) {
            if (com_bases[i] != 0) sprintf(line, "COM%d %04X", i + 1,
                                           com_bases[i]);
            else                   sprintf(line, "COM%d ----", i + 1);
            ui_puts(21 + i * 13, 2, line,
                    com_bases[i] ? UI_ATTR(C_LGREEN, C_BLUE) : A_HINT);
        }

        ui_box(10, 4, 60, 11, A_FRAME);
        ui_puts(12, 4, " Link setup ", A_TITLE);
        sprintf(line, "Port    <  COM%d  (base %04X)  >",
                com_index + 1, com_bases[com_index]);
        ui_puts(13, 6, line, field == 0 ? A_ITEMSEL : A_ITEM);
        sprintf(line, "Speed   <  %lu bps 8N1  >    ", speeds[speed_index]);
        ui_puts(13, 8, line, field == 1 ? A_ITEMSEL : A_ITEM);
        sprintf(line, "Role    <  %s  >          ",
                role ? "RECEIVE files" : "SEND files   ");
        ui_puts(13, 10, line, field == 2 ? A_ITEMSEL : A_ITEM);
        ui_puts(13, 12, "Both machines must use the SAME speed.", A_HINT);
        ui_puts(13, 13, "Start the RECEIVE side first, then the SEND side.",
                A_HINT);

        ui_puts(4, 16, "Cable: null-modem (crossover) - TD/RD swapped, "
                "RTS/CTS swapped,", A_HINT);
        ui_puts(4, 17, "DTR/DSR swapped, ground to ground.  A straight "
                "cable will not work.", A_HINT);
        sprintf(line, "Working directory: %.55s", curdir);
        ui_puts(4, 19, line, UI_ATTR(C_LCYAN, C_BLUE));
        ui_puts(4, 20, "Polled I/O only - no interrupt is hooked and no "
                "TSR is installed.", A_HINT);
        ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
        ui_puts(0, SCR_H - 1, " Up/Down Field   Left/Right Change   "
                "Enter Start   Esc Quit", A_STATUS);

        key = ui_getkey();
        if (key == KEY_ESC || key == KEY_F10) return 0;
        if (key == KEY_ENTER) return 1;
        if (key == KEY_UP)        { if (field > 0) field--; }
        else if (key == KEY_DOWN) { if (field < 2) field++; }
        else if (key == KEY_TAB)  { field = (field + 1) % 3; }
        else if (key == KEY_LEFT || key == KEY_RIGHT) {
            int step = (key == KEY_RIGHT) ? 1 : 3;      /* 3 == -1 mod 4 */
            if (field == 0) {
                do { com_index = (com_index + step) & 3; }
                while (com_bases[com_index] == 0);      /* skip absent   */
            } else if (field == 1) {
                speed_index = (speed_index + step) & 3;
            } else {
                role = !role;
            }
        }
    }
}

/* --- Send side ------------------------------------------------------------- */

static int name_cmp(const void *a, const void *b)
{
    return strcmp(((const FENT *)a)->name, ((const FENT *)b)->name);
}

static void read_dir(void)
{
    nent = 0;
    if (dirw_first("*.*") == 0) {
        do {
            if (dirw_isdir()) continue;
            if (nent < MAX_ENT) {
                strncpy(ents[nent].name, dirw_name(), NAMELEN - 1);
                ents[nent].name[NAMELEN - 1] = '\0';
                ents[nent].size = dirw_size();
                ents[nent].tag = 0;
                nent++;
            }
        } while (dirw_next() == 0);
    }
    qsort(ents, (size_t)nent, sizeof(FENT), name_cmp);
}

static void tag_totals(int *files, unsigned long *bytes)
{
    int i;
    *files = 0;
    *bytes = 0UL;
    for (i = 0; i < nent; i++)
        if (ents[i].tag) { (*files)++; *bytes += ents[i].size; }
}

#define LIST_Y 5
#define VIS   16

/* Tag the files to send.  Returns 1 when F5 starts a transfer, 0 on Esc. */
static int pick_files(void)
{
    char line[80];
    int sel = 0, top = 0, key, row, gi, tf, i;
    unsigned long tb;

    read_dir();
    for (;;) {
        if (sel < 0) sel = 0;
        if (sel >= nent) sel = nent ? nent - 1 : 0;
        if (sel < top) top = sel;
        if (sel >= top + VIS) top = sel - VIS + 1;

        ui_cls(A_DESKTOP);
        ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
        ui_puts(2, 0, "CASTALIA LINK", A_TITLE);
        ui_puts(18, 0, "Send files", UI_ATTR(C_WHITE, C_BLUE));
        sprintf(line, "From: %-40.40s", curdir);
        ui_puts(2, 2, line, UI_ATTR(C_YELLOW, C_BLUE));
        tag_totals(&tf, &tb);
        sprintf(line, "Tagged: %d file(s), %lu KB   over COM%d at %lu bps",
                tf, tb / 1024UL, com_index + 1, speeds[speed_index]);
        ui_puts(2, 3, line, UI_ATTR(C_LGREEN, C_BLUE));

        ui_box(0, 4, SCR_W, 19, A_FRAME);
        for (row = 0; row < VIS; row++) {
            gi = top + row;
            ui_fill(1, LIST_Y + row, SCR_W - 2, 1, ' ', A_DESKTOP);
            if (gi >= nent) continue;
            if (gi == sel)
                ui_fill(1, LIST_Y + row, SCR_W - 2, 1, ' ', A_ITEMSEL);
            sprintf(line, "%c %-13.13s %12lu", ents[gi].tag ? 0x10 : ' ',
                    ents[gi].name, ents[gi].size);
            ui_puts(2, LIST_Y + row, line, (gi == sel) ? A_ITEMSEL
                    : (ents[gi].tag ? UI_ATTR(C_LGREEN, C_BLUE) : A_ITEM));
        }
        if (nent == 0)
            ui_puts(3, LIST_Y, "(no files in this directory)", A_HINT);
        ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
        ui_puts(0, SCR_H - 1,
                " Space Tag   * All   - None   F5 SEND   Esc Back", A_STATUS);

        key = ui_getkey();
        if (key == KEY_ESC) return 0;
        else if (key == KEY_UP)   { if (sel > 0) sel--; }
        else if (key == KEY_DOWN) { if (sel < nent - 1) sel++; }
        else if (key == KEY_PGUP) { sel -= VIS; }
        else if (key == KEY_PGDN) { sel += VIS; }
        else if (key == KEY_HOME) { sel = 0; }
        else if (key == KEY_END)  { sel = nent - 1; }
        else if (key == KEY_SPACE || key == KEY_INS) {
            if (nent) {
                ents[sel].tag = !ents[sel].tag;
                if (sel < nent - 1) sel++;
            }
        } else if (key == '*') {
            for (i = 0; i < nent; i++) ents[i].tag = 1;
        } else if (key == '-') {
            for (i = 0; i < nent; i++) ents[i].tag = 0;
        } else if (key == KEY_F5) {
            tag_totals(&tf, &tb);
            if (tf == 0 && nent > 0) ents[sel].tag = 1;  /* the cursor one */
            tag_totals(&tf, &tb);
            if (tf == 0) notify("Nothing to send.",
                                "Tag files with Space first.");
            else return 1;
        }
    }
}

/* Send one file.  Returns SER_OK, or a negative code that stops the job. */
static int send_one(const char *name, unsigned long fsize)
{
    char msg[80];
    FILE *fp;
    unsigned long done = 0UL;
    unsigned fcrc = 0xFFFFu;
    int rc, n;

    fp = fopen(name, "rb");
    if (fp == NULL) {
        sprintf(msg, "SKIPPED %s - cannot open for reading", name);
        log_add(msg);
        if (n_bad < 32000) n_bad++;
        tot_done += fsize;                  /* keep the total bar honest */
        return SER_OK;
    }
    put_u32(frm + HDRLEN, fsize);
    strcpy((char *)(frm + HDRLEN + 4), name);
    rc = send_frame(F_FILE, 4 + (int)strlen(name) + 1);
    if (rc != SER_OK) { fclose(fp); return rc; }
    sprintf(msg, "sending   %-13.13s %lu bytes", name, fsize);
    log_add(msg);
    xfer_update(name, 0UL, fsize);

    for (;;) {
        n = (int)fread(frm + HDRLEN, 1, (size_t)MAXPAY, fp);
        if (n <= 0) break;
        fcrc = crc16_upd(fcrc, frm + HDRLEN, n);
        rc = send_frame(F_DATA, n);
        if (rc != SER_OK) { fclose(fp); return rc; }
        done += (unsigned long)n;
        tot_done += (unsigned long)n;
        xfer_update(name, done, fsize);
    }
    if (ferror(fp)) {
        /* No EOF for this file.  The EOF carries the size and CRC of what
         * was SENT, so it would verify a truncated copy as good; without
         * it the receiver deletes the partial file when the next FILE
         * header or the DONE arrives. */
        sprintf(msg, "READ ERROR on %s at %lu bytes - not sent", name, done);
        log_add(msg);
        if (n_bad < 32000) n_bad++;
        if (fsize > done)
            tot_done += fsize - done;       /* keep the total bar honest */
        fclose(fp);
        return SER_OK;
    }
    fclose(fp);

    put_u32(frm + HDRLEN, done);            /* EOF carries size + CRC   */
    put_u16(frm + HDRLEN + 4, fcrc);
    rc = send_frame(F_EOF, 6);
    if (rc == SER_OK) { files_done++; xfer_update(name, done, fsize); }
    return rc;
}

static void do_send(void)
{
    char msg[80];
    int rc = SER_OK, i, len = 0;

    tx_seq = rx_seq = 0;        /* a fresh session starts at bit 0 */

    tag_totals(&tot_files, &tot_bytes);
    tot_done = 0UL;
    files_done = 0;
    n_retry = n_crc = n_line = n_bad = n_renamed = 0;
    log_used = 0;
    log_dirty = 1;
    t_start = ui_ticks();
    xfer_screen("Sending - shaking hands...");
    xfer_update("", 0UL, 0UL);

    memcpy(frm + HDRLEN, "CASTLINK", 8);
    frm[HDRLEN + 8] = (unsigned char)PROTO_VER;
    frm[HDRLEN + 9] = (unsigned char)'S';
    put_u16(frm + HDRLEN + 10, (unsigned)tot_files);
    put_u32(frm + HDRLEN + 12, tot_bytes);
    if (send_frame(F_HELLO, 16) != SER_OK) {
        notify("No answer from the other keep.",
               "Check the cable and the COM port, and that the far end "
               "is in RECEIVE mode.");
        return;
    }
    rc = recv_frame(&len, TMO_IDLE);
    if (rc != F_HELLO || len < 9 ||
        memcmp(frm + HDRLEN, "CASTLINK", 8) != 0) {
        notify("The far end did not answer with a CASTLINK hello.",
               "Both machines must run CASTLINK at the same speed.");
        return;
    }
    if (frm[HDRLEN + 8] != PROTO_VER) {
        sprintf(msg, "The far end speaks protocol %d, this build speaks %d.",
                (int)frm[HDRLEN + 8], PROTO_VER);
        notify(msg, "Use matching CASTALIA DOS releases on both machines.");
        return;
    }

    xfer_screen("Sending");
    log_add("link up - protocol 1, peer ready");
    t_start = ui_ticks();
    rc = SER_OK;
    for (i = 0; i < nent; i++) {
        if (!ents[i].tag) continue;
        rc = send_one(ents[i].name, ents[i].size);
        if (rc != SER_OK) break;
    }
    if (rc == SER_OK) {
        rc = send_frame(F_DONE, 0);
        summary(rc == SER_OK ? "Transfer complete."
                : "All files sent, but the far end went quiet.");
    } else {
        (void)send_frame(F_ABORT, 0);       /* best effort; may fail too */
        summary(rc == SER_ABORT ? "Cancelled - Esc pressed."
                : rc == SER_TIMEOUT
                  ? "Link timed out - cable or partner lost."
                  : "Link failed after repeated retries.");
    }
}

/* --- Receive side ----------------------------------------------------------- */

/* The "waiting for the other keep" state, with a live spinner so it is
 * obvious the machine is alive and listening rather than wedged. */
static void wait_screen(unsigned long waited)
{
    static const char spin[4] = { '|', '/', '-', '\\' };
    char line[80], el[16];
    int w = 62, h = 9, x = (SCR_W - w) / 2, y = 6;

    ui_fill(x, y, w, h, ' ', A_PANEL);
    ui_box(x, y, w, h, A_PANEL);
    ui_fill(x + 1, y, w - 2, 1, ' ', A_PANELHDR);
    ui_puts(x + 2, y, " Waiting for the other keep ", A_PANELHDR);
    fmt_elapsed(el, waited);
    sprintf(line, "%c  listening on COM%d at %lu bps ... %s",
            spin[(int)((waited / 5UL) & 3UL)], com_index + 1,
            speeds[speed_index], el);
    ui_putlim(x + 3, y + 2, line, w - 6, A_PANEL);
    ui_putlim(x + 3, y + 4, "Start CASTLINK on the sending machine, pick "
              "the same", w - 6, A_PANEL);
    ui_putlim(x + 3, y + 5, "speed, tag the files and press F5.  Esc gives "
              "up.", w - 6, A_PANEL);
    sprintf(line, "Files land in %.44s", curdir);
    ui_putlim(x + 3, y + 7, line, w - 6, A_PANEL);
}

/* Why a DOS file call failed: the INT 24h code if there was one (write
 * protect, drive not ready, ...), else the usual suspect 'dflt'. */
static const char *why_failed(const char *dflt)
{
    int code = ui_crit_take();
    return (code >= 0) ? ui_crit_text(code) : dflt;
}

/* The file this run created under 'name' never verified: delete it, so
 * a truncated or corrupt copy cannot pass for the real thing later. */
static void drop_partial(const char *name, int *created, const char *why)
{
    char msg[80];

    if (!*created) return;
    *created = 0;
    remove(name);
    sprintf(msg, "NOT SAVED  %-13.13s - %s, removed", name, why);
    log_add(msg);
    if (n_bad < 32000) n_bad++;
}

/* Handle one FILE header: name it safely, never clobber, open it.
 * '*created' is set while 'name' is a file of ours not yet verified. */
static void begin_file(FILE **out, char *name, int len,
                       unsigned long *fsize, unsigned long *fdone,
                       unsigned *fcrc, int *created)
{
    char msg[80];
    int un;

    if (len < 5) {                          /* size word + at least "x\0" */
        log_add("malformed FILE header ignored");
        if (n_bad < 32000) n_bad++;
        return;
    }
    if (*out != NULL) {                     /* sender skipped an EOF     */
        fclose(*out);
        *out = NULL;
    }
    drop_partial(name, created, "no EOF");
    *fsize = get_u32(frm + HDRLEN);
    frm[HDRLEN + len - 1] = 0;              /* payload is NUL-terminated */
    strncpy(name, (const char *)(frm + HDRLEN + 4), NAMELEN - 1);
    name[NAMELEN - 1] = '\0';
    sanitize_name(name);
    *fdone = 0UL;
    *fcrc = 0xFFFFu;
    un = unique_name(name);
    if (un < 0) {
        /* The data still has to be ACKed and read off the wire, but it
         * goes nowhere: *out stays NULL and *created 0, so nothing of
         * the user's is touched when this file's EOF arrives. */
        sprintf(msg, "REFUSED %s - it and ~1..~99 all exist", name);
        log_add(msg);
        if (n_bad < 32000) n_bad++;
        return;
    }
    if (un > 0) {
        n_renamed++;
        sprintf(msg, "exists already -> saving as %s", name);
        log_add(msg);
    }
    (void)ui_crit_take();                   /* forget older errors       */
    *out = fopen(name, "wb");
    if (*out == NULL) {
        sprintf(msg, "CANNOT WRITE %s - %s", name,
                why_failed("disk full or read-only?"));
        log_add(msg);
        if (n_bad < 32000) n_bad++;
    } else {
        *created = 1;
        sprintf(msg, "receiving %-13.13s %lu bytes", name, *fsize);
        log_add(msg);
    }
}

static void do_receive(void)
{
    const char *verdict = "Transfer complete.";
    char name[NAMELEN + 8], msg[80];
    FILE *out = NULL;
    unsigned long fsize = 0UL, fdone = 0UL, t_wait;
    unsigned fcrc = 0xFFFFu;
    int rc, len = 0, running = 1, quiet = 0;
    int wfail = 0;                  /* this file could not be written  */
    int created = 0;                /* 'name' is ours and not verified */

    tx_seq = rx_seq = 0;        /* a fresh session starts at bit 0 */

    tot_files = files_done = 0;
    tot_bytes = tot_done = 0UL;
    n_retry = n_crc = n_line = n_bad = n_renamed = 0;
    log_used = 0;
    log_dirty = 1;
    strcpy(name, "");
    xfer_screen("Receiving - waiting for a partner");
    t_wait = ui_ticks();

    /* Wait for the sender's HELLO in short slices, so the spinner ticks
     * and Esc is honoured - the machine never looks hung. */
    for (;;) {
        wait_screen(ticks_since(t_wait));
        rc = recv_frame(&len, TMO_SLICE);
        if (rc == SER_ABORT || rc == F_ABORT) return;
        if (rc == F_HELLO && len >= 16 &&
            memcmp(frm + HDRLEN, "CASTLINK", 8) == 0) break;
        /* SER_TIMEOUT / SER_BAD / a stray frame: keep listening. */
    }
    if (frm[HDRLEN + 8] != PROTO_VER) {
        sprintf(msg, "The far end speaks protocol %d, this build speaks %d.",
                (int)frm[HDRLEN + 8], PROTO_VER);
        notify(msg, "Use matching CASTALIA DOS releases on both machines.");
        return;
    }
    tot_files = (int)get_u16(frm + HDRLEN + 10);
    tot_bytes = get_u32(frm + HDRLEN + 12);

    memcpy(frm + HDRLEN, "CASTLINK", 8);    /* greet back               */
    frm[HDRLEN + 8] = (unsigned char)PROTO_VER;
    frm[HDRLEN + 9] = (unsigned char)'R';
    put_u16(frm + HDRLEN + 10, 0u);
    put_u32(frm + HDRLEN + 12, 0UL);
    if (send_frame(F_HELLO, 16) != SER_OK) {
        notify("The far end stopped answering during the handshake.",
               "Check the cable and try again.");
        return;
    }

    xfer_screen("Receiving");
    log_add("link up - protocol 1, sender ready");
    t_start = ui_ticks();
    xfer_update("", 0UL, 0UL);

    while (running) {
        rc = recv_frame(&len, TMO_IDLE);
        if (rc == SER_BAD) continue;        /* NAK sent; sender retries  */
        if (rc == SER_ABORT) {
            verdict = "Cancelled - Esc pressed.";
            (void)send_frame(F_ABORT, 0);
            break;
        }
        if (rc == SER_TIMEOUT) {
            if (++quiet >= 3) {
                verdict = "Link timed out - cable or partner lost.";
                break;
            }
            log_add("quiet line - still waiting for the sender");
            xfer_update(name, fdone, fsize);
            continue;
        }
        if (rc < 0) { verdict = "Link failed."; break; }
        quiet = 0;

        if (rc == F_FILE) {
            begin_file(&out, name, len, &fsize, &fdone, &fcrc, &created);
            wfail = (out == NULL);      /* could not even be created   */
            xfer_update(name, 0UL, fsize);
        } else if (rc == F_DATA) {
            if (out != NULL &&
                (int)fwrite(frm + HDRLEN, 1, (size_t)len, out) != len) {
                fclose(out);
                out = NULL;
                wfail = 1;
                sprintf(msg, "WRITE FAILED on %s - %s", name,
                        why_failed("disk full?"));
                log_add(msg);
            }
            fcrc = crc16_upd(fcrc, frm + HDRLEN, len);
            fdone += (unsigned long)len;
            tot_done += (unsigned long)len;
            xfer_update(name, fdone, fsize);
        } else if (rc == F_EOF) {
            unsigned long claimed = get_u32(frm + HDRLEN);
            unsigned wantcrc = get_u16(frm + HDRLEN + 4);
            if (out != NULL) {
                /* The C library and DOS hold the tail of the file in
                 * their buffers until the close, so a full disk can first
                 * show up here; the file is "ok" only if the close is. */
                if (fclose(out) != 0) {
                    wfail = 1;
                    sprintf(msg, "CLOSE FAILED on %s - %s", name,
                            why_failed("disk full?"));
                    log_add(msg);
                }
                out = NULL;
            }
            if (wfail) {
                /* The bytes arrived intact - fdone and fcrc track what
                 * came off the wire - but they never reached the disk,
                 * so the sender's EOF figures would still match and this
                 * would have been logged "ok".  Say what happened and
                 * take the partial file away, as CASTCOPY does. */
                if (created) {
                    drop_partial(name, &created, "write failed");
                } else {
                    sprintf(msg, "NOT SAVED  %-13.13s - nothing written",
                            name);
                    log_add(msg);
                }
            } else if (!created || claimed != fdone || wantcrc != fcrc) {
                /* A corrupt copy under the real name would be taken for
                 * the real file later, so it goes (see the header).
                 * !created: an EOF with no FILE header before it. */
                sprintf(msg, "VERIFY FAILED on %s (%lu of %lu bytes)",
                        name, fdone, claimed);
                log_add(msg);
                if (created)
                    drop_partial(name, &created, "bad CRC");
                else if (n_bad < 32000)
                    n_bad++;
            } else {
                created = 0;                /* verified: it stays        */
                files_done++;
                sprintf(msg, "ok        %-13.13s %lu bytes", name, fdone);
                log_add(msg);
            }
            wfail = 0;
            xfer_update(name, fdone, fsize);
        } else if (rc == F_DONE) {
            running = 0;
        } else if (rc == F_ABORT) {
            verdict = "The far end cancelled the transfer.";
            running = 0;
        }
        /* Any other type is ignored - it was ACKed, which is harmless. */
    }

    if (out != NULL)                        /* interrupted mid-file      */
        fclose(out);
    drop_partial(name, &created, "interrupted");
    if (tot_files < files_done) tot_files = files_done;
    summary(verdict);
}

/* --- Main --------------------------------------------------------------------- */

/* Under the test seam this is not the program's entry point: the test
 * file supplies main().  Keeping it a real (non-static) function means
 * every helper above stays referenced, so the test build compiles with
 * the same warnings-as-errors as the DOS build. */
#ifdef CASTLINK_TEST
int castlink_main(void)
#else
int main(void)
#endif
{
    scan_com();
    get_cwd(curdir);
    ui_init();
    for (;;) {
        if (!setup_screen()) break;
        uart_open();
        if (role == 0) {
            if (pick_files()) do_send();
        } else {
            do_receive();
        }
        uart_close();
    }
    uart_close();
    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
