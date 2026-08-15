/* ===================================================================
 * CDPLAYER.C  -  CASTALIA DOS Red Book Audio CD Player  (CDPLAYER.EXE)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * A full-screen text-mode player for AUDIO (Red Book) compact discs,
 * driven entirely through MSCDEX / SHSUCDX (the CD-ROM extension loaded
 * by the Castalia "CDROM" boot profile).  It reads the disc's table of
 * contents once, lists the tracks with their lengths, and lets you
 * play / pause / skip / stop through the drive's own audio hardware.
 *
 * How it talks to the drive (see Ralf Brown's Interrupt List, INT 2Fh):
 *   - INT 2Fh AX=1500h  installation check -> number of CD drives.
 *   - INT 2Fh AX=1510h  "send device request": CX = CD-ROM drive number,
 *     ES:BX -> a CD-ROM device request header we hand-assemble.
 * DOS device request headers are byte-packed, so we build them as a raw
 * unsigned-char buffer with explicit little-endian writes at documented
 * offsets - never as a C struct (which the compiler may pad).
 *
 * This tool cannot be exercised on real hardware in CI, so every request
 * is built faithfully to the documented spec, every returned status word
 * is checked, and every path degrades gracefully (no busy-waits, no
 * hangs) when MSCDEX is absent or the disc has no audio.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os cdplayer.c ..\common\ui.c
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include "../common/UI.H"

/* --- MSCDEX request/status constants -------------------------------- */

/* Device request header status WORD (offset 3) bits. */
#define STAT_ERROR 0x8000U      /* bit 15: driver flagged an error       */
#define STAT_DONE  0x0200U      /* bit 9 : request completed (documented) */

/* Device driver command codes (request header offset 2). */
#define CMD_IOCTL_INPUT  3      /* read an IOCTL control block            */
#define CMD_PLAY_AUDIO   0x84   /* 132: start Red Book audio playback     */
#define CMD_STOP_AUDIO   0x85   /* 133: stop / pause playback             */
#define CMD_RESUME_AUDIO 0x88   /* 136: resume after a stop/pause         */

/* IOCTL control-block subfunction codes (first byte of the xfer buffer). */
#define CB_DISK_INFO   0x0A     /* lowest/highest track + lead-out        */
#define CB_TRACK_INFO  0x0B     /* one track's start address + control    */
#define CB_QCHANNEL    0x0C     /* current head position (elapsed time)   */
#define CB_STATUS      0x0F     /* audio playing/paused + last play range */

/* A CD holds up to 99 tracks; index arrays by absolute track number and
 * keep the lead-out at [high+1], so a couple of guard slots are added. */
#define TRK_MAX 99

/* Transport state machine. */
#define ST_STOPPED 0
#define ST_PLAYING 1
#define ST_PAUSED  2

/* Data-track test.  In the Q sub-channel the ADR/CONTROL byte packs the
 * 4-bit CONTROL field in the HIGH nibble; CONTROL bit 2 (=> byte 0x40)
 * marks a data track rather than audio.  Getting this "wrong" for an
 * exotic disc only mislabels a row - it never breaks playback control. */
#define TRACK_IS_DATA(c) (((c) & 0x40) != 0)

/* --- Fixed buffers and player state (no dynamic allocation) --------- */

static unsigned char g_req[32];     /* CD-ROM device request header      */
static unsigned char g_xfer[64];    /* IOCTL control-block transfer area */

static int g_drive;                 /* 0-based CD-ROM drive number (CX)  */
static int g_low, g_high;           /* lowest / highest track numbers    */
static int g_naudio, g_ndata;       /* audio / data track counts         */
static unsigned long g_start[TRK_MAX + 3]; /* raw Red Book start per track*/
static unsigned char g_ctrl[TRK_MAX + 3];  /* ADR/CONTROL byte per track  */

static int g_state;                 /* ST_STOPPED / ST_PLAYING / ST_PAUSED*/
static int g_sel;                   /* highlighted track number          */
static int g_cur;                   /* track being played / paused       */
static int g_drivestat;             /* -1 n/a, 0 idle, 1 playing, 2 paused*/
static unsigned g_elapsed_secs;     /* elapsed time shown for current trk */
static unsigned long g_play_base;   /* seconds accumulated before segment */
static unsigned long g_seg_tick;    /* BIOS tick at current segment start */
static char g_msg[64];              /* transient status/hint message      */

/* --- Little-endian field helpers ------------------------------------
 * DOS request headers are packed with no alignment, so we place each
 * multi-byte value one byte at a time, low byte first. */

static void store16(unsigned char *p, int off, unsigned v)
{
    p[off]     = (unsigned char)(v & 0xFFU);
    p[off + 1] = (unsigned char)((v >> 8) & 0xFFU);
}

static void store32(unsigned char *p, int off, unsigned long v)
{
    p[off]     = (unsigned char)(v & 0xFFUL);
    p[off + 1] = (unsigned char)((v >> 8) & 0xFFUL);
    p[off + 2] = (unsigned char)((v >> 16) & 0xFFUL);
    p[off + 3] = (unsigned char)((v >> 24) & 0xFFUL);
}

static unsigned long load32(const unsigned char *p, int off)
{
    return (unsigned long)p[off]
         | ((unsigned long)p[off + 1] << 8)
         | ((unsigned long)p[off + 2] << 16)
         | ((unsigned long)p[off + 3] << 24);
}

/* Red Book address DWORD is packed low->high as Frame, Second, Minute, 0.
 * Convert to an absolute frame count (75 frames per second). */
static unsigned long rb_to_frames(unsigned long rb)
{
    unsigned long fr  =  rb        & 0xFFUL;   /* byte 0: frame  */
    unsigned long sec = (rb >> 8)  & 0xFFUL;   /* byte 1: second */
    unsigned long min = (rb >> 16) & 0xFFUL;   /* byte 2: minute */
    return (min * 60UL + sec) * 75UL + fr;
}

static unsigned long track_len_frames(int t)
{
    unsigned long a = rb_to_frames(g_start[t]);
    unsigned long b = rb_to_frames(g_start[t + 1]); /* next start / lead-out */
    return (b < a) ? 0UL : b - a;
}

static unsigned long total_frames(void)
{
    unsigned long a = rb_to_frames(g_start[g_low]);
    unsigned long b = rb_to_frames(g_start[g_high + 1]);
    return (b < a) ? 0UL : b - a;
}

/* --- The MSCDEX request header ---------------------------------------
 * Layout common to every CD-ROM device request:
 *   offset 0  BYTE  length of this header (bytes)
 *   offset 1  BYTE  subunit (the CD-ROM drive number within the driver)
 *   offset 2  BYTE  command code
 *   offset 3  WORD  status (returned by the driver)
 *   offset 5  8 x BYTE  reserved (zero)
 *   offset 13 ..       command-specific fields
 */
static void hdr_init(int len, int cmd)
{
    memset(g_req, 0, sizeof(g_req));
    g_req[0] = (unsigned char)len;      /* 0: total header length       */
    g_req[1] = (unsigned char)g_drive;  /* 1: subunit = CD drive number */
    g_req[2] = (unsigned char)cmd;      /* 2: command code              */
    /* 3-4: status WORD left 0 (driver fills it);  5-12: reserved 0.    */
}

/* Issue the request already built in g_req via INT 2Fh AX=1510h.
 * ES:BX must point at the header; CX carries the CD-ROM drive number.
 * We load ES/BX with the far segment:offset of the buffer.  Returns 1 on
 * success, 0 if the driver set the error bit in the status WORD. */
static int cd_request(void)
{
    union REGS in, out;
    struct SREGS s;
    unsigned status;

    segread(&s);                                 /* start from real segs  */
    s.es    = FP_SEG((void far *)&g_req[0]);      /* ES -> header segment  */
    in.x.bx = FP_OFF((void far *)&g_req[0]);      /* BX -> header offset   */
    in.x.ax = 0x1510;                             /* send device request   */
    in.x.cx = (unsigned)g_drive;                  /* CD-ROM drive number   */
    int86x(0x2F, &in, &out, &s);

    /* Read back the status WORD at offset 3 (bit 15 = error, 9 = done). */
    status = (unsigned)g_req[3] | ((unsigned)g_req[4] << 8);
    return (status & STAT_ERROR) ? 0 : 1;
}

/* IOCTL INPUT (command 3): read a control block from the drive.
 * The transfer buffer's first byte is the subfunction code; for track
 * info its second byte carries the track number we want. */
static int cd_ioctl_input(int cbcode, int cbsize, int track)
{
    unsigned seg, off;

    memset(g_xfer, 0, sizeof(g_xfer));
    g_xfer[0] = (unsigned char)cbcode;          /* control-block code    */
    if (cbcode == CB_TRACK_INFO)
        g_xfer[1] = (unsigned char)track;       /* input: track number   */

    hdr_init(26, CMD_IOCTL_INPUT);
    g_req[13] = 0;                              /* 13: media descriptor   */
    off = FP_OFF((void far *)&g_xfer[0]);
    seg = FP_SEG((void far *)&g_xfer[0]);
    store16(g_req, 14, off);                    /* 14: xfer far pointer   */
    store16(g_req, 16, seg);                    /* 16:   (offset,segment) */
    store16(g_req, 18, (unsigned)cbsize);       /* 18: byte count         */
    store16(g_req, 20, 0);                      /* 20: start sector (n/a) */
    store32(g_req, 22, 0UL);                    /* 22: volume-id ptr none */
    return cd_request();
}

/* PLAY AUDIO (command 132): play 'frames' sectors from a Red Book start. */
static int cd_play(unsigned long redbook_addr, unsigned long frames)
{
    hdr_init(22, CMD_PLAY_AUDIO);
    g_req[13] = 1;                              /* 13: 1 = Red Book mode  */
    store32(g_req, 14, redbook_addr);           /* 14: start Red Book addr*/
    store32(g_req, 18, frames);                 /* 18: frames to play     */
    return cd_request();
}

/* STOP (133) / RESUME (136): no command-specific fields. */
static int cd_simple(int cmd)
{
    hdr_init(13, cmd);
    return cd_request();
}

/* Current-position (Q-channel) read -> seconds within the current track. */
static int cd_qchannel(unsigned *secs)
{
    unsigned mn, sc;
    if (!cd_ioctl_input(CB_QCHANNEL, 11, 0))
        return 0;
    /* Q-channel block (code preserved at byte 0):
     *   1 control/ADR  2 track  3 index
     *   4 MIN  5 SEC  6 FRAME   (running time WITHIN the current track)
     *   8 AMIN 9 ASEC 10 AFRAME (absolute time on disc)
     * Some drives report BCD here; we display the values as returned. */
    mn = g_xfer[4];
    sc = g_xfer[5];
    *secs = mn * 60U + sc;
    return 1;
}

/* Audio status -> status WORD (bit 0 set => paused). */
static int cd_audio_status(unsigned *word)
{
    if (!cd_ioctl_input(CB_STATUS, 11, 0))
        return 0;
    /* byte 0 = 0Fh, bytes 1-2 = status WORD, 3-6 = start of last play,
     * 7-10 = end of last play (both Red Book). */
    *word = (unsigned)g_xfer[1] | ((unsigned)g_xfer[2] << 8);
    return 1;
}

/* --- Detection ------------------------------------------------------- */

/* INT 2Fh AX=1500h installation check.  BX is left at 0 before the call;
 * if MSCDEX is present it returns the CD drive count in BX and the first
 * CD-ROM drive index in CX.  BX unchanged (0) => no CD-ROM driver. */
static int detect_cdrom(void)
{
    union REGS r;
    r.x.ax = 0x1500;
    r.x.bx = 0x0000;
    int86(0x2F, &r, &r);
    if (r.x.bx == 0)
        return 0;
    g_drive = (int)r.x.cx;      /* index of the first CD-ROM drive letter */
    return 1;
}

/* Read the whole table of contents once.  Returns 1 on a usable TOC. */
static int read_toc(void)
{
    int t;

    if (!cd_ioctl_input(CB_DISK_INFO, 8, 0))
        return 0;
    /* Disk-info block: 1 = lowest track, 2 = highest track,
     * 3-6 = Red Book start of the lead-out. */
    g_low  = g_xfer[1];
    g_high = g_xfer[2];
    if (g_high < g_low || g_high == 0)
        return 0;
    if (g_low < 1)        g_low = 1;
    if (g_high > TRK_MAX) g_high = TRK_MAX;
    g_start[g_high + 1] = load32(g_xfer, 3);    /* lead-out address       */

    g_naudio = 0;
    g_ndata = 0;
    for (t = g_low; t <= g_high; t++) {
        if (cd_ioctl_input(CB_TRACK_INFO, 8, t)) {
            /* Track-info block: 2-5 = Red Book start, 6 = ADR/CONTROL. */
            g_start[t] = load32(g_xfer, 2);
            g_ctrl[t]  = g_xfer[6];
        } else {
            g_start[t] = g_start[g_high + 1];   /* unknown -> zero length */
            g_ctrl[t]  = 0;
        }
        if (TRACK_IS_DATA(g_ctrl[t]))
            g_ndata++;
        else
            g_naudio++;
    }
    return 1;
}

/* --- Transport actions ---------------------------------------------- */

static unsigned long elapsed_secs(void)
{
    unsigned long now = ui_ticks();
    if (now < g_seg_tick)                 /* BIOS tick wrapped at midnight */
        g_seg_tick = now;
    return g_play_base + (now - g_seg_tick) * 10UL / 182UL;  /* /18.2 */
}

static void act_play_track(int t)
{
    unsigned long len;
    if (t < g_low || t > g_high)
        return;
    if (TRACK_IS_DATA(g_ctrl[t])) {
        sprintf(g_msg, "Track %d is a data track (no audio).", t);
        return;
    }
    len = track_len_frames(t);
    if (cd_play(g_start[t], len)) {
        g_cur = t;
        g_state = ST_PLAYING;
        g_play_base = 0UL;
        g_seg_tick = ui_ticks();
        g_elapsed_secs = 0;
        sprintf(g_msg, "Playing track %d.", t);
    } else {
        sprintf(g_msg, "Drive did not accept PLAY for track %d.", t);
    }
}

static void toggle_play(void)
{
    if (TRACK_IS_DATA(g_ctrl[g_sel])) {
        sprintf(g_msg, "Track %d is a data track (no audio).", g_sel);
        return;
    }
    if (g_state != ST_STOPPED && g_cur == g_sel) {
        if (g_state == ST_PLAYING) {
            g_play_base = elapsed_secs();       /* freeze elapsed clock   */
            if (cd_simple(CMD_STOP_AUDIO)) {
                g_state = ST_PAUSED;
                strcpy(g_msg, "Paused.");
            } else {
                strcpy(g_msg, "Drive refused PAUSE.");
            }
        } else {                                /* was paused -> resume   */
            if (cd_simple(CMD_RESUME_AUDIO)) {
                g_state = ST_PLAYING;
                g_seg_tick = ui_ticks();        /* restart segment clock  */
                strcpy(g_msg, "Resumed.");
            } else {
                strcpy(g_msg, "Drive refused RESUME.");
            }
        }
    } else {
        act_play_track(g_sel);
    }
}

/* Find the next audio (non-data) track at/after 'start' in direction dir. */
static int find_audio(int start, int dir)
{
    int t = start;
    while (t >= g_low && t <= g_high) {
        if (!TRACK_IS_DATA(g_ctrl[t]))
            return t;
        t += dir;
    }
    return -1;
}

static void skip_track(int dir)
{
    int t = find_audio(g_sel + dir, dir);
    if (t < 0) {
        strcpy(g_msg, "No further audio track.");
        return;
    }
    g_sel = t;
    act_play_track(t);          /* transport skip also starts playback   */
}

static void stop_all(void)
{
    if (g_state != ST_STOPPED)
        cd_simple(CMD_STOP_AUDIO);
    g_state = ST_STOPPED;
    g_drivestat = 0;
}

/* One polling step while a track plays: refresh the readout and detect
 * the natural end of the track.  Elapsed time is shown from the drive's
 * Q-channel when available; end-of-track is judged from real elapsed
 * time (reliable at audio's fixed 1x rate) so we never hang waiting. */
static void poll_playback(void)
{
    unsigned long e, len;
    unsigned q = 0, w = 0;

    if (cd_qchannel(&q))
        g_elapsed_secs = q;
    else
        g_elapsed_secs = (unsigned)elapsed_secs();

    if (cd_audio_status(&w))
        g_drivestat = (w & 0x0001U) ? 2 : 1;
    else
        g_drivestat = -1;

    e = elapsed_secs();
    len = track_len_frames(g_cur) / 75UL;
    if (len > 0UL && e >= len) {
        cd_simple(CMD_STOP_AUDIO);
        g_state = ST_STOPPED;
        g_drivestat = 0;
        sprintf(g_msg, "Track %d finished.", g_cur);
    }
}

/* --- Rendering ------------------------------------------------------- */

static void draw_static(void)
{
    char t[32];
    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA CD PLAYER", A_TITLE);
    sprintf(t, "Drive %c:", (char)('A' + g_drive));
    ui_puts(SCR_W - 10, 0, t, UI_ATTR(C_WHITE, C_BLUE));

    ui_box(2, 2, 46, 20, A_FRAME);
    ui_puts(4, 2, " Tracks ", A_TITLE);
    ui_box(50, 2, 28, 14, A_FRAME);
    ui_puts(52, 2, " Now Playing ", A_TITLE);

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(2, SCR_H - 1,
        "Up/Dn Select  Enter/Spc Play/Pause  N/P Skip  S Stop  Esc Quit",
        A_STATUS);
}

static void draw_dynamic(void)
{
    char line[64];
    int visible = 18;
    int ntr = g_high - g_low + 1;
    int top, i, t;
    unsigned long lf, ls;
    unsigned mm, ss;

    /* --- windowed track list, keeping the selection on screen --- */
    if (ntr <= visible) {
        top = g_low;
    } else {
        top = g_sel - visible / 2;
        if (top < g_low)
            top = g_low;
        if (top > g_high - visible + 1)
            top = g_high - visible + 1;
    }
    for (i = 0; i < visible; i++) {
        int y = 3 + i;
        unsigned char attr;
        char mark;
        t = top + i;
        if (t > g_high) {
            ui_fill(3, y, 44, 1, ' ', A_ITEM);
            continue;
        }
        attr = (t == g_sel) ? A_ITEMSEL : A_ITEM;
        mark = (t == g_cur && g_state != ST_STOPPED) ? '>' : ' ';
        lf = track_len_frames(t);
        ls = lf / 75UL;
        mm = (unsigned)(ls / 60UL);
        ss = (unsigned)(ls % 60UL);
        ui_fill(3, y, 44, 1, ' ', attr);
        sprintf(line, "%c Track %2d   %2u:%02u   %s",
                mark, t, mm, ss,
                TRACK_IS_DATA(g_ctrl[t]) ? "DATA" : "audio");
        ui_putlim(3, y, line, 44, attr);
    }

    /* --- now-playing panel --- */
    {
        int px = 52, py = 4;
        const char *st;
        ui_fill(51, 3, 26, 12, ' ', A_DESKTOP);

        if (g_state == ST_PLAYING)      st = "Playing";
        else if (g_state == ST_PAUSED)  st = "Paused ";
        else                            st = "Stopped";

        if (g_state == ST_STOPPED)
            strcpy(line, "Track: --");
        else
            sprintf(line, "Track: %d", g_cur);
        ui_puts(px, py, line, A_TITLE);

        sprintf(line, "State: %s", st);
        ui_puts(px, py + 1, line, UI_ATTR(C_WHITE, C_BLUE));

        {
            unsigned emm = g_elapsed_secs / 60;
            unsigned ess = g_elapsed_secs % 60;
            unsigned long tl = (g_state == ST_STOPPED)
                             ? 0UL : track_len_frames(g_cur) / 75UL;
            unsigned lmm = (unsigned)(tl / 60UL);
            unsigned lss = (unsigned)(tl % 60UL);
            sprintf(line, "Time:  %2u:%02u / %2u:%02u", emm, ess, lmm, lss);
            ui_puts(px, py + 2, line, UI_ATTR(C_YELLOW, C_BLUE));
        }

        {
            const char *ds;
            if (g_drivestat == 1)      ds = "playing";
            else if (g_drivestat == 2) ds = "paused";
            else if (g_drivestat == 0) ds = "idle";
            else                       ds = "n/a";
            sprintf(line, "Drive: %s", ds);
            ui_puts(px, py + 3, line, A_HINT);
        }

        sprintf(line, "Audio:%2d  Data:%2d", g_naudio, g_ndata);
        ui_puts(px, py + 5, line, UI_ATTR(C_LGRAY, C_BLUE));

        {
            unsigned long tot = total_frames() / 75UL;
            unsigned tmm = (unsigned)(tot / 60UL);
            unsigned tss = (unsigned)(tot % 60UL);
            sprintf(line, "Total: %2u:%02u", tmm, tss);
            ui_puts(px, py + 6, line, UI_ATTR(C_LGRAY, C_BLUE));
        }
    }

    /* --- message / hint line --- */
    ui_fill(2, 22, 76, 1, ' ', A_HINT);
    if (g_msg[0])
        ui_putlim(2, 22, g_msg, 76, A_HINT);
}

/* --- A small centred message screen for the degenerate cases -------- */

static void message_screen(const char *l1, const char *l2)
{
    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA CD PLAYER", A_TITLE);
    ui_box(14, 9, 52, 7, A_FRAME);
    ui_puts(16, 11, l1, UI_ATTR(C_WHITE, C_BLUE));
    ui_puts(16, 13, l2, A_TITLE);
    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(2, SCR_H - 1, "Press any key to exit.", A_STATUS);
    ui_getkey();
}

/* Returns 1 to quit the player. */
static int handle_key(int k)
{
    switch (k) {
    case KEY_UP:
        if (g_sel > g_low) g_sel--;
        break;
    case KEY_DOWN:
        if (g_sel < g_high) g_sel++;
        break;
    case KEY_ENTER:
    case KEY_SPACE:
        toggle_play();
        break;
    case KEY_RIGHT:
    case 'n': case 'N':
        skip_track(1);
        break;
    case KEY_LEFT:
    case 'p': case 'P':
        skip_track(-1);
        break;
    case 's': case 'S':
        stop_all();
        strcpy(g_msg, "Stopped.");
        break;
    case KEY_ESC:
        stop_all();
        return 1;
    default:
        break;
    }
    return 0;
}

/* --- Test seam: CDPLAYER_TEST -----------------------------------------
 * tests/dos/CDTEST.C #includes this file with -DCDPLAYER_TEST so that
 * the MSCDEX layer below - detection, the TOC read, play, stop, the
 * Q-channel - can be driven headlessly inside an emulator with a CUE/BIN
 * disc mounted, and the results written to a file the host reads back.
 *
 * The seam is a renamed main() rather than a copy of the driver calls in
 * the test, because a copy would only ever prove that the copy works.
 * It is the same convention CASTLINK.C uses.  No DOS build of CDPLAYER
 * defines CDPLAYER_TEST.
 * -------------------------------------------------------------------- */
#ifdef CDPLAYER_TEST
int cdplayer_main(void)
#else
int main(void)
#endif
{
    unsigned long next;
    int k, quit, t;

    ui_init();
    ui_cls(A_DESKTOP);

    if (!detect_cdrom()) {
        message_screen("No CD-ROM driver (MSCDEX) detected.",
                       "Boot the CDROM profile to play audio discs.");
        ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
        ui_done();
        return 0;
    }

    if (!read_toc()) {
        message_screen("No audio disc, or the TOC could not be read.",
                       "Insert an audio CD and try again.");
        ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
        ui_done();
        return 0;
    }

    /* Start with the first audio track selected. */
    g_sel = g_low;
    for (t = g_low; t <= g_high; t++) {
        if (!TRACK_IS_DATA(g_ctrl[t])) {
            g_sel = t;
            break;
        }
    }
    g_state = ST_STOPPED;
    g_cur = g_sel;
    g_drivestat = -1;
    g_elapsed_secs = 0;
    g_play_base = 0UL;
    g_seg_tick = ui_ticks();
    if (g_naudio == 0)
        strcpy(g_msg, "No audio tracks on this disc.");
    else
        g_msg[0] = '\0';

    draw_static();
    draw_dynamic();

    quit = 0;
    while (!quit) {
        /* Pace ~2 refreshes/second, yielding to DOS, waking early on a
         * key.  Never a raw busy-spin (ui_idle lets TSRs / the emulator
         * breathe); bounded so a silent drive can never hang us. */
        next = ui_ticks() + 9UL;
        while (ui_ticks() < next && !ui_keywaiting()) {
            if (ui_ticks() + 20UL < next)   /* midnight-wrap guard */
                next = ui_ticks();
            ui_idle();
        }
        if (ui_keywaiting()) {
            k = ui_getkey();
            quit = handle_key(k);
        } else if (g_state == ST_PLAYING) {
            poll_playback();
        }
        draw_dynamic();
    }

    /* Always silence the drive on the way out so audio does not keep
     * playing after we return to the shell. */
    cd_simple(CMD_STOP_AUDIO);
    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
