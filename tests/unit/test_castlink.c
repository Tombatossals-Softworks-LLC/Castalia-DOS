/* ===================================================================
 * test_castlink.c  -  host unit tests for the CASTLINK frame layer
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * CASTLINK's wire protocol is the one part of Castalia that can never
 * be tried out here: it needs two machines and a null-modem cable, so
 * every claim it makes - that a frame arrives intact, that a corrupted
 * header is caught, that a retransmission is not written to disk twice
 * - would otherwise rest on reading the code.  These tests replace the
 * cable with a virtual UART.
 *
 * CASTLINK.C is #included (not linked) and compiled with -DCASTLINK_TEST
 * so the static frame layer - send_frame(), recv_frame(), crc16_upd()
 * and the frm[] buffer itself - is in reach, and so its pin()/pout()
 * land on the two hooks below instead of on hardware.  A frame is built
 * by asking the real send_frame() for one and catching what it writes;
 * it is then played back into the real recv_frame(), corrupted first
 * where a test wants to see the damage caught.  The fake clock advances
 * on every read so the no-hang timeouts still fire in finite time.
 *
 * Build & run (see scripts/test-unit.sh):
 *   gcc -x c -std=c89 -Wall -Wextra -Werror -Dfar= -Dnear= \
 *       -DCASTLINK_TEST -Ici/stubs -Isrc/common \
 *       -o test_castlink tests/unit/test_castlink.c && ./test_castlink
 * =================================================================== */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include "UI.H"
#include "DIRW.H"

/* --- the fake clock, in place of the BIOS tick ----------------------
 * It moves one tick per reading, which is what the polled loops need:
 * a stalled clock would turn a timeout into an endless spin. */
static unsigned long fake_now = 1000UL;
unsigned long ui_ticks(void) { return ++fake_now; }
void ui_idle(void) { }

/* Nobody presses Esc in here, so no transfer is ever cancelled. */
int  ui_keywaiting(void) { return 0; }
int  ui_getkey(void) { return KEY_ESC; }

/* The screen is not what is under test: every drawing call is inert. */
void ui_init(void) { }
void ui_done(void) { }
void ui_cls(unsigned char attr) { (void)attr; }
void ui_puts(int x, int y, const char *s, unsigned char attr)
{ (void)x; (void)y; (void)s; (void)attr; }
void ui_putlim(int x, int y, const char *s, int maxlen, unsigned char attr)
{ (void)x; (void)y; (void)s; (void)maxlen; (void)attr; }
void ui_fill(int x, int y, int w, int h, char ch, unsigned char attr)
{ (void)x; (void)y; (void)w; (void)h; (void)ch; (void)attr; }
void ui_box(int x, int y, int w, int h, unsigned char attr)
{ (void)x; (void)y; (void)w; (void)h; (void)attr; }
void ui_hline(int x, int y, int w, unsigned char attr)
{ (void)x; (void)y; (void)w; (void)attr; }
void ui_hbar(int x, int y, int w, int permille_,
             unsigned char attr, unsigned char dimattr)
{ (void)x; (void)y; (void)w; (void)permille_; (void)attr; (void)dimattr; }

/* No real disk sits behind the receiver here, so no INT 24h error is
 * ever pending; the file-error messages only need something to print. */
int ui_crit_take(void) { return -1; }
const char *ui_crit_text(int code) { (void)code; return "disk error"; }

/* DOS services and the directory walker are reached only from
 * castlink_main(), which no test calls; they exist so the program links. */
int int86(int intno, union REGS *inregs, union REGS *outregs)
{ (void)intno; (void)inregs; (void)outregs; return 0; }
int int86x(int intno, union REGS *inregs, union REGS *outregs,
           struct SREGS *segregs)
{ (void)intno; (void)inregs; (void)outregs; (void)segregs; return 0; }
void segread(struct SREGS *sregs) { (void)sregs; }

int  dirw_first(const char *mask) { (void)mask; return 1; }
int  dirw_next(void) { return 1; }
const char   *dirw_name(void) { return ""; }
unsigned long dirw_size(void) { return 0UL; }
int  dirw_isdir(void) { return 0; }

#include "../../src/castlink/CASTLINK.C"

/* --- the virtual UART ------------------------------------------------
 * pin()/pout() arrive here through the CASTLINK_TEST seam.  rxq holds
 * what the far end "sent"; txq captures every byte CASTLINK writes to
 * the transmit holding register, which is how the ACK/NAK it answers
 * with can be inspected.  The transmitter is always ready; data-ready
 * is set only while unread bytes remain. */
#define VU_MAX 8192
static unsigned char rxq[VU_MAX];
static int rxn = 0, rxi = 0;
static unsigned char txq[VU_MAX];
static int txn = 0;

unsigned char castlink_port_in(unsigned port)
{
    unsigned reg = port & 7u;
    if (reg == (unsigned)U_LSR)
        return (unsigned char)(LSR_THRE | (rxi < rxn ? LSR_DR : 0));
    if (reg == (unsigned)U_RBR)
        return (rxi < rxn) ? rxq[rxi++] : (unsigned char)0;
    return 0;
}

void castlink_port_out(unsigned port, unsigned char v)
{
    if ((port & 7u) == (unsigned)U_RBR && txn < VU_MAX) txq[txn++] = v;
}

static void wire_reset(void) { rxn = 0; rxi = 0; txn = 0; }

static void wire_feed(const unsigned char *b, int n)
{
    if (rxn + n > VU_MAX) return;
    memcpy(rxq + rxn, b, (size_t)n);
    rxn += n;
}

/* How many times byte 'b' was answered on the wire. */
static int wire_count(int b)
{
    int i, n = 0;
    for (i = 0; i < txn; i++)
        if (txq[i] == (unsigned char)b) n++;
    return n;
}

static int acks(void) { return wire_count(ACK | 0) + wire_count(ACK | 1); }

/* --- test scaffolding ------------------------------------------------ */

static int checks = 0, failures = 0;

static void ck(const char *what, int cond)
{
    checks++;
    if (cond) {
        printf("  [  OK  ] %s\n", what);
    } else {
        printf("  [ FAIL ] %s\n", what);
        failures++;
    }
}

/* The payload every built frame carries: a recognisable A..Z ramp. */
static int payload_ok(int len)
{
    int i;
    for (i = 0; i < len; i++)
        if (frm[HDRLEN + i] != (unsigned char)('A' + (i % 26))) return 0;
    return 1;
}

/* Build one real frame: preload the ACK the far end would answer with,
 * let send_frame() do the framing, and keep what went out on the wire. */
static int build_frame(int type, int len, int seq, unsigned char *out)
{
    unsigned char ackb;
    int i;

    for (i = 0; i < len; i++)
        frm[HDRLEN + i] = (unsigned char)('A' + (i % 26));
    wire_reset();
    ackb = (unsigned char)(ACK | seq);
    wire_feed(&ackb, 1);
    tx_seq = seq;
    if (send_frame(type, len) != SER_OK) return -1;
    memcpy(out, txq, (size_t)txn);
    return txn;
}

int main(void)
{
    unsigned char f0[FRMMAX], f1[FRMMAX], tmp[FRMMAX], hdr[HDRLEN], ackb;
    int n0, n1, rc, len = 0;

    uart = 0x3F8;                   /* any base: the vUART decodes A0-A2 */

    printf("== CASTLINK: a frame survives a clean round trip ==\n");
    n0 = build_frame(F_DATA, 64, 0, f0);
    ck("send_frame put a whole frame on the wire", n0 == HDRLEN + 64 + 2);
    ck("it opens with SOH", f0[0] == SOH);
    ck("the type byte carries the type, seq 0", f0[1] == (unsigned char)F_DATA);
    ck("the length word is little-endian", f0[2] == 64 && f0[3] == 0);

    wire_reset();
    wire_feed(f0, n0);
    rx_seq = 0;
    memset(frm, 0, sizeof(frm));    /* so a pass proves it came off the wire */
    rc = recv_frame(&len, TMO_IDLE);
    ck("recv_frame hands up the frame type", rc == F_DATA);
    ck("the payload length survives", len == 64);
    ck("every payload byte survives", payload_ok(64));
    ck("the frame is acknowledged once, with its own seq",
       wire_count(ACK | 0) == 1 && wire_count(ACK | 1) == 0);

    printf("\n== CASTLINK: the CRC covers the header, not just the payload ==\n");
    ck("the CRC word spans type, length and payload",
       get_u16(f0 + HDRLEN + 64) == crc16_upd(0xFFFFu, f0 + 1, 3 + 64));
    ck("...which is not what a payload-only CRC would be",
       get_u16(f0 + HDRLEN + 64) != crc16_upd(0xFFFFu, f0 + HDRLEN, 64));

    memcpy(tmp, f0, (size_t)n0);
    tmp[1] = (unsigned char)F_EOF;  /* a bit error turning DATA into EOF  */
    wire_reset();
    wire_feed(tmp, n0);
    rx_seq = 0;
    rc = recv_frame(&len, TMO_IDLE);
    ck("a flipped type byte is caught", rc == SER_BAD);
    ck("...it is never handed up as an EOF", rc != F_EOF);
    ck("...and the sender is NAKed, not ACKed",
       wire_count(NAK) == 1 && acks() == 0);

    /* A flipped length byte desynchronises the framing, so the receiver
     * needs bytes to keep reading; feed it a second frame so it can
     * finish and actually reach the CRC test.  The property is that a
     * damaged frame is never handed up as a good one. */
    memcpy(tmp, f0, (size_t)n0);
    tmp[2] ^= 0x01;
    wire_reset();
    wire_feed(tmp, n0);
    wire_feed(f0, n0);
    rx_seq = 0;
    rc = recv_frame(&len, TMO_IDLE);
    ck("a flipped length byte is never accepted as a good frame",
       rc != F_DATA);
    ck("...it is reported as a bad frame", rc == SER_BAD);

    printf("\n== CASTLINK: an impossible length is refused, not obeyed ==\n");
    hdr[0] = SOH;
    hdr[1] = (unsigned char)F_DATA;
    put_u16(hdr + 2, (unsigned)(MAXPAY + 1));
    wire_reset();
    wire_feed(hdr, HDRLEN);
    wire_feed(f0, n0);
    rx_seq = 0;
    memset(frm + HDRLEN, 0xEE, (size_t)MAXPAY);   /* canary in the buffer */
    rc = recv_frame(&len, TMO_IDLE);
    ck("a length past MAXPAY is refused", rc == SER_BAD);
    ck("...with no byte written into the payload buffer",
       frm[HDRLEN] == 0xEE && frm[FRMMAX - 3] == 0xEE);
    ck("...and the sender is NAKed", wire_count(NAK) == 1);
    rc = recv_frame(&len, TMO_IDLE);
    ck("...and the receiver resynchronises on the next frame",
       rc == F_DATA && len == 64);

    printf("\n== CASTLINK: a retransmission is acknowledged, not delivered twice ==\n");
    wire_reset();
    wire_feed(f0, n0);              /* seq 0                              */
    wire_feed(f0, n0);              /* the same frame again: a lost ACK   */
    rx_seq = 0;
    rc = recv_frame(&len, TMO_SLICE);
    ck("the first copy is delivered", rc == F_DATA && len == 64);
    rc = recv_frame(&len, TMO_SLICE);
    ck("the duplicate is NOT delivered a second time", rc != F_DATA);
    ck("...the receiver simply runs out of frames", rc == SER_TIMEOUT);
    ck("...yet both copies were acknowledged", acks() == 2);

    printf("\n== CASTLINK: the sequence bit alternates ==\n");
    n1 = build_frame(F_DATA, 32, 1, f1);
    ck("a seq-1 frame is built", n1 == HDRLEN + 32 + 2);
    ck("the seq bit rides in the type byte",
       (f1[1] & SEQ_BIT) != 0 && (f1[1] & 0x7F) == F_DATA);
    wire_reset();
    wire_feed(f0, n0);
    wire_feed(f1, n1);
    rx_seq = 0;
    rc = recv_frame(&len, TMO_IDLE);
    ck("the seq-0 frame is delivered", rc == F_DATA && len == 64);
    rc = recv_frame(&len, TMO_IDLE);
    ck("the seq-1 frame is delivered too", rc == F_DATA && len == 32);
    ck("each was acknowledged with its own sequence bit",
       wire_count(ACK | 0) == 1 && wire_count(ACK | 1) == 1);

    printf("\n== CASTLINK: only the right ACK retires a frame ==\n");
    n_retry = 0;
    n_crc = 0;
    wire_reset();
    ackb = (unsigned char)(ACK | 1);        /* the ACK for the other one */
    wire_feed(&ackb, 1);
    tx_seq = 0;
    rc = send_frame(F_DATA, 8);
    ck("an ACK for the other sequence does not retire the frame",
       rc == SER_FAIL);
    ck("...it gave up after MAX_RETRY attempts", n_retry == MAX_RETRY);
    ck("...having put the frame on the wire every time",
       txn == MAX_RETRY * (HDRLEN + 8 + 2));
    ck("...and the sequence bit did not advance", tx_seq == 0);

    n_retry = 0;
    n_crc = 0;
    wire_reset();
    tmp[0] = NAK;                           /* rejected, then accepted   */
    tmp[1] = (unsigned char)(ACK | 0);
    wire_feed(tmp, 2);
    tx_seq = 0;
    rc = send_frame(F_DATA, 8);
    ck("a NAKed frame is sent again and then accepted", rc == SER_OK);
    ck("...counted as one retry and one CRC error",
       n_retry == 1 && n_crc == 1);
    ck("...and the sequence bit then advances", tx_seq == 1);

    printf("\n== CASTLINK: CRC-16/CCITT is the standard one ==\n");
    ck("the check vector holds: crc16(\"123456789\") == 0x29B1",
       crc16_upd(0xFFFFu, (const unsigned char *)"123456789", 9) == 0x29B1u);
    ck("an empty span leaves the running CRC alone",
       crc16_upd(0xFFFFu, (const unsigned char *)"123456789", 0) == 0xFFFFu);
    ck("it resumes, as the whole-file CRC needs it to",
       crc16_upd(crc16_upd(0xFFFFu, (const unsigned char *)"1234", 4),
                 (const unsigned char *)"56789", 5) == 0x29B1u);

    printf("\n%d checks, %d failure(s)\n", checks, failures);
    if (failures) {
        printf("CASTLINK TESTS FAILED\n");
        return 1;
    }
    printf("ALL CASTLINK TESTS PASSED\n");
    return 0;
}
