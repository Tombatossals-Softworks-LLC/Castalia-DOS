/* ===================================================================
 * CASTMARK.C  -  CASTALIA MARK: System Inspector & Benchmark
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * The Castalia answer to CheckIt / Norton SI / Landmark: a hardware
 * inspector plus five benchmarks (CPU integer, FPU, memory copy, text
 * video, disk read) with animated gradient bars, a provisional
 * "Castalia Index" (386SX/16 = 100), and a saved-score comparison so
 * you can measure profile changes (CLEAN vs XMS) or hardware upgrades.
 *
 * Design rules:
 *   - Timing uses the BIOS tick counter (18.2 Hz) - no CPU-speed guess.
 *   - Fixed-duration runs (~1.5 s each): iterations are counted, so the
 *     same code measures a 386SX and a Pentium fairly.
 *   - CPU/FPU detection comes from the shared CPUDET module (real
 *     FNINIT/FNSTSW probe under Open Watcom - meaningful on a 386+387).
 *   - Baseline constants below are PROVISIONAL calibration anchors to
 *     tune on real hardware; raw numbers are always shown.
 *
 * Build (Open Watcom):
 *   wcl -0 -bt=dos -ml -os castmark.c ..\common\ini.c ..\common\ui.c
 *       ..\common\cpudet.c   (cpudet compiled -3; see Makefile)
 *
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include "../common/UI.H"
#include "../common/INI.H"
#include "../common/CPUDET.H"
#include "../common/XMSINFO.H"
#include "../common/VIDDET.H"

#define NBENCH     5
#define DUR_TICKS  28L          /* ~1.54 s per benchmark               */

/* Calibration anchors: the reference 386SX/16 with a 387SX fitted.
 *
 * MEASURED, not estimated.  These are a real CASTMARK run on an 86Box
 * machine configured to match the project's reference 386SX (4 MB, 387
 * present, CLEAN profile).  Everything before them was guesswork, and
 * the guesses were wrong by up to 17x - the FPU anchor in particular
 * assumed no coprocessor, which made any machine with one look absurd.
 *
 * By construction the reference machine now scores exactly 100.
 *
 * Provenance and confidence, per anchor:
 *   CPU/FPU/memory/video  86Box emulates 386-class timing cycle by
 *                         cycle, so these should hold on metal.
 *   disk read             the least transferable of the five: it is an
 *                         emulated IDE image, and a real drive or a
 *                         CompactFlash card behaves differently.  Worth
 *                         re-measuring on the real machine.
 *
 * A machine with no coprocessor SKIPS the FPU benchmark rather than
 * scoring zero on it, so the index stays meaningful there - it is then
 * the geometric mean of the other four. */
static const long bench_base[NBENCH] = {
    38L,        /* CPU integer, k-iterations/s                          */
    136L,       /* FPU,        kFLOP/s (mixed x, /, +) - WITH a 387SX   */
    6656L,      /* memory copy, KB/s                                    */
    201L,       /* text video,  screens/s x10  (20.1 screens/s)         */
    530L        /* disk read,   KB/s                                    */
};

static const char *bench_name[NBENCH] = {
    "CPU integer", "FPU (80x87)", "Memory copy", "Video (text)",
    "Disk read"
};
static const char *bench_unit[NBENCH] = {
    "kOps/s", "kFLOP/s", "KB/s", "scr/s", "KB/s"
};

static long results[NBENCH];
static long prev[NBENCH];
static long prev_index = -1;
static int  have_run = 0;
static int  g_fpu = 0;

/* --- Log scale --------------------------------------------------------
 * The first version of this screen scaled the bars linearly to 4x the
 * 386SX baseline and clamped the index's per-benchmark ratio at 400%.
 * On the first machine faster than a 486 that produced four full bars
 * out of five and an index pinned near its ceiling: a 486 and a Pentium
 * would have scored the same.  Ratios spanning three orders of
 * magnitude want a log scale and a geometric mean, so that is what this
 * uses.  BAR_LO/BAR_HI are octaves either side of the baseline. */
#define BAR_LO  (-2)            /* left end  = 1/4 x the 386SX          */
#define BAR_HI  ( 5)            /* right end =  32 x the 386SX          */
#define BAR_MK_W 27             /* bar columns; the rest holds "xNNN"   */

/* log2 of a 1/256 fixed-point number, result also in 1/256ths of an
 * octave.  log2_fx(256) == 0 because 256 represents 1.0, so callers
 * pass a ratio already scaled by 256 and get octaves relative to the
 * baseline - no further offset.
 *
 * The mantissa is interpolated linearly over each octave; worst-case
 * error is 0.086 octaves, about one character on a 27-column bar.
 * exp2_fx below inverts it to the 9 significant bits the form carries
 * (exactly up to 2x, within 1/256 relative above), which is far finer
 * than anything the screen shows. */
static long log2_fx(unsigned long x)
{
    long e = 0;

    if (x == 0UL)
        return 0L;
    while (x >= 0x200UL) { x >>= 1; e++; }
    while (x <  0x100UL) { x <<= 1; e--; }
    /* 256 <= x < 512: the octave is e, the fraction is (x-256)/256. */
    return e * 256L + (long)(x - 0x100UL);
}

/* 2^(y/256), scaled by 256: the inverse of log2_fx. */
static long exp2_fx(long y)
{
    long e = y >> 8;                    /* arithmetic shift: floors */
    long v = 0x100L + (y - (e << 8));

    while (e > 0) { v <<= 1;  e--; }
    while (e < 0) { v >>= 1;  e++; }
    return v;
}

/* Benchmark result as a multiple of the 386SX baseline, in 1/256ths.
 * 256 = exactly baseline.  Returns -1 when there is no result. */
static long ratio_fx(int i)
{
    if (results[i] < 0)
        return -1L;
    return (results[i] * 256L) / bench_base[i];
}

/* Optimizer sinks: keep benchmark work observable. */
static volatile long   g_isink = 0;
static double          g_dsink = 0.0;

/* --- Safe hardware queries (same conventions as HWINFO) -------------- */

static unsigned conv_kb(void)
{
    union REGS r;
    int86(0x12, &r, &r);
    return r.x.ax;
}

static int has_ems(void)
{
    FILE *fp = fopen("EMMXXXX0", "rb");
    if (fp == NULL)
        return 0;
    fclose(fp);
    return 1;
}

static unsigned equip_word(void)
{
    union REGS r;
    int86(0x11, &r, &r);
    return r.x.ax;
}

static void dos_version(int *major, int *minor)
{
    union REGS r;
    r.h.ah = 0x30;
    int86(0x21, &r, &r);
    *major = r.h.al;
    *minor = r.h.ah;
}

/* --- Layout ----------------------------------------------------------
 *  y0   title bar
 *  2-14 Inspector box (x1,w39)   |   Marks box (x41,w38)
 *  16-22 Run box (x1,w78)
 *  y24  status bar
 * -------------------------------------------------------------------- */

#define RUN_Y     16
#define BAR_X     4
#define BAR_W     72

/* 'out' must hold FMT_VALUE_MAX: a full-width long is 11 characters, and
 * the scr/s form prints two of them either side of a point plus the unit.
 * This is a benchmark - the whole point is that it runs on machines
 * nobody has measured yet - so the buffer is sized for what the format
 * can produce, not for what a 386SX produces. */
#define FMT_VALUE_MAX 32

static void fmt_value(int idx, long v, char *out)
{
    if (v < 0) {
        strcpy(out, "skipped");
    } else if (idx == 3) {
        sprintf(out, "%ld.%ld %s", v / 10L, v % 10L, bench_unit[idx]);
    } else {
        sprintf(out, "%ld %s", v, bench_unit[idx]);
    }
}

/* "x14.9", "x152", "x0.4" - the benchmark as a multiple of the 386SX
 * baseline.  One decimal below 10x, none above, so it always fits in
 * the five columns to the right of the bar. */
static void fmt_ratio(long rf, char *out)
{
    if (rf < 0L) {
        strcpy(out, "  -  ");
    } else if (rf >= 10L * 256L) {
        sprintf(out, "x%ld", (rf + 128L) / 256L);
    } else {
        long t = (rf * 10L + 128L) / 256L;      /* tenths */
        sprintf(out, "x%ld.%ld", t / 10L, t % 10L);
    }
}

static void draw_mark(int i)
{
    int ny = 3 + i * 2;
    char buf[64], val[FMT_VALUE_MAX];
    long v = results[i];
    int pm;

    ui_fill(42, ny,     36, 1, ' ', A_DESKTOP);
    ui_fill(42, ny + 1, 36, 1, ' ', A_DESKTOP);

    ui_puts(43, ny, bench_name[i], UI_ATTR(C_WHITE, C_BLUE));

    if (!have_run && v == 0) {
        ui_puts(60, ny, "---", A_HINT);
        ui_hbar(43, ny + 1, BAR_MK_W, 0, A_TITLE, A_HINT);
        return;
    }
    fmt_value(i, v, val);
    sprintf(buf, "%16s", val);
    ui_puts(60, ny, buf + (strlen(buf) > 16 ? strlen(buf) - 16 : 0),
            UI_ATTR(C_YELLOW, C_BLUE));

    if (v >= 0 && prev[i] > 0) {
        long d = v - prev[i];
        char dc;
        unsigned char da;
        if (d > 0)      { dc = (char)0x18; da = UI_ATTR(C_LGREEN, C_BLUE); }
        else if (d < 0) { dc = (char)0x19; da = UI_ATTR(C_LRED, C_BLUE); }
        else            { dc = '=';        da = A_HINT; }
        sprintf(buf, "%c%ld", dc, (d < 0) ? -d : d);
        ui_puts(43 + 12, ny, buf, da);
    }

    /* Bar: log scale from 1/4x to 32x the 386SX/16 baseline, with the
     * multiplier printed at the right so a saturated bar still carries
     * the number. */
    if (v < 0) {
        ui_hbar(43, ny + 1, BAR_MK_W, 0, A_TITLE, A_HINT);
    } else {
        long rf = ratio_fx(i);
        long lg, span;

        if (rf <= 0) {
            pm = 0;
        } else {
            lg   = log2_fx((unsigned long)rf);  /* octaves */
            span = (long)(BAR_HI - BAR_LO) * 256L;
            pm   = (int)(((lg - (long)BAR_LO * 256L) * 1000L) / span);
            if (pm < 0)    pm = 0;
            if (pm > 1000) pm = 1000;
        }
        ui_hbar(43, ny + 1, BAR_MK_W, pm, A_TITLE, A_HINT);

        fmt_ratio(rf, buf);
        ui_puts(43 + BAR_MK_W + 2, ny + 1, buf,
                (rf >= 256L) ? UI_ATTR(C_LGREEN, C_BLUE)
                             : UI_ATTR(C_LRED, C_BLUE));
    }
}

static void draw_inspector(void)
{
    char v[48];
    int y = 3, dmaj, dmin;
    unsigned eq = equip_word(), ek;
    const char *env;

    ui_puts(3, y, "Processor", A_ITEM);
    /* The inspector column clips at 35; the long 386 string is 36. */
    cpu_describe_short(v);
    ui_putlim(3, y + 1, v, 35, UI_ATTR(C_YELLOW, C_BLUE));
    y += 2;

    sprintf(v, "Coprocessor  %s (%s)",
            g_fpu ? "PRESENT" : "none",
            fpu_probe_kind() ? "FNSTSW probe" : "BIOS flag");
    ui_putlim(3, y++, v, 36, g_fpu ? UI_ATTR(C_LGREEN, C_BLUE) : A_ITEM);

    sprintf(v, "Conventional %u KB", conv_kb());
    ui_puts(3, y++, v, A_ITEM);

    {
        /* The XMS driver owns extended memory once loaded, and answers
         * the BIOS call with 0 on purpose - ask the driver. */
        int src = XMEM_NONE;
        ek = mem_ext_kb(&src);
        if (src == XMEM_XMS)
            sprintf(v, "Extended     %u KB free (XMS)", ek);
        else if (src == XMEM_BIOS)
            sprintf(v, "Extended     %u KB (INT 15h)", ek);
        else
            strcpy(v, "Extended     none reported");
    }
    ui_puts(3, y++, v, A_ITEM);

    sprintf(v, "XMS %s   EMS %s", xms_present() ? "yes" : "no",
            has_ems() ? "yes" : "no");
    ui_puts(3, y++, v, A_ITEM);

    sprintf(v, "Video        %s", vid_name(vid_class()));
    ui_puts(3, y++, v, A_ITEM);

    if (eq & 0x0001)
        sprintf(v, "Floppies     %u drive(s)",
                (unsigned)(((eq >> 6) & 3) + 1));
    else
        strcpy(v, "Floppies     none reported");
    ui_puts(3, y++, v, A_ITEM);

    sprintf(v, "Ports        COM:%u  LPT:%u",
            (unsigned)((eq >> 9) & 7), (unsigned)((eq >> 14) & 3));
    ui_puts(3, y++, v, A_ITEM);

    dos_version(&dmaj, &dmin);
    sprintf(v, "DOS          %d.%02d (FreeDOS core)", dmaj, dmin);
    ui_puts(3, y++, v, A_ITEM);

    env = getenv("CASTPROFILE");
    sprintf(v, "Profile      %s", (env && env[0]) ? env : "(unknown)");
    ui_puts(3, y++, v, A_ITEM);
}

/* The Castalia Index: the GEOMETRIC mean of the five per-benchmark
 * multipliers, times 100, so a 386SX/16 scores exactly 100.
 *
 * It used to be the arithmetic mean of the ratios with each one clamped
 * at 400%.  Two things were wrong with that.  The clamp meant every
 * machine faster than roughly a 486 landed in the high 300s regardless
 * of how fast it actually was.  And an arithmetic mean of normalised
 * ratios is dominated by whichever benchmark happens to have the
 * smallest baseline: with a real 80387 in the machine the FPU ratio
 * alone reaches 150x, which would swamp the other four.  The geometric
 * mean is the standard answer to both, and it needs no clamp.
 *
 * Returns -1 when there is nothing to average. */
static long castalia_index(void)
{
    long sum = 0L;
    int  i, n = 0;

    for (i = 0; i < NBENCH; i++) {
        long rf = ratio_fx(i);
        if (rf > 0L) {
            sum += log2_fx((unsigned long)rf);
            n++;
        }
    }
    if (n == 0)
        return -1L;
    return (exp2_fx(sum / (long)n) * 100L) / 256L;
}

static void draw_index(void)
{
    long idx;
    char buf[64];

    if (!have_run)
        return;
    idx = castalia_index();
    if (idx < 0L)
        return;

    sprintf(buf, "CASTALIA INDEX: %ld", idx);
    ui_puts(BAR_X, RUN_Y + 4, buf, A_TITLE);
    if (prev_index > 0) {
        long d = idx - prev_index;
        sprintf(buf, "(prev %ld, %c%ld)", prev_index,
                (d >= 0) ? '+' : '-', (d < 0) ? -d : d);
        ui_puts(BAR_X + 22, RUN_Y + 4, buf, A_HINT);
    }
    ui_puts(BAR_X + 40, RUN_Y + 4,
            "geometric mean; 386SX/16 = 100", A_HINT);
}

static void draw_screen(void)
{
    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA MARK", A_TITLE);
    ui_puts(17, 0, "System Inspector & Benchmark", UI_ATTR(C_WHITE, C_BLUE));

    ui_box(1, 2, 39, 13, A_FRAME);
    ui_puts(3, 2, " Inspector ", A_TITLE);
    draw_inspector();

    ui_box(41, 2, 38, 13, A_FRAME);
    ui_puts(43, 2, " Benchmarks ", A_TITLE);
    {
        int i;
        for (i = 0; i < NBENCH; i++)
            draw_mark(i);
    }

    ui_box(1, RUN_Y, 78, 7, A_FRAME);
    ui_puts(3, RUN_Y, " Run ", A_TITLE);
    if (!have_run)
        ui_puts(BAR_X, RUN_Y + 2,
                "Press ENTER to run the full benchmark suite (~8 seconds).",
                A_ITEM);
    draw_index();

    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(2, SCR_H - 1,
            " Enter Run all   S Save scores   Esc Exit", A_STATUS);
}

/* --- Progress (drawn inside the Run box) ----------------------------- */

static int  g_lastpm = -1;

static void prog_begin(int idx)
{
    char buf[64];
    ui_fill(2, RUN_Y + 1, 76, 5, ' ', A_DESKTOP);
    sprintf(buf, "Running %d/%d: %s ...", idx + 1, NBENCH, bench_name[idx]);
    ui_puts(BAR_X, RUN_Y + 1, buf, UI_ATTR(C_WHITE, C_BLUE));
    ui_hbar(BAR_X, RUN_Y + 2, BAR_W, 0, A_TITLE, A_HINT);
    g_lastpm = -1;
}

static void prog_update(long elapsed, long live, const char *unit)
{
    int pm = (int)(elapsed * 1000L / DUR_TICKS);
    char buf[48];
    if (pm > 1000) pm = 1000;
    if (pm == g_lastpm)
        return;
    g_lastpm = pm;
    ui_hbar(BAR_X, RUN_Y + 2, BAR_W, pm, A_TITLE, A_HINT);
    sprintf(buf, "%ld %s        ", live, unit);
    ui_puts(BAR_X, RUN_Y + 3, buf, A_HINT);
}

/* Elapsed ticks with a single-wrap guard (midnight rollover). */
static long ticks_since(unsigned long t0)
{
    unsigned long now = ui_ticks();
    if (now >= t0)
        return (long)(now - t0);
    return (long)now + 1L;      /* wrapped: close enough for a 1.5 s run */
}

/* --- The benchmarks --------------------------------------------------- */

/* Thousands of `units` per second, from a count and a BIOS-tick span.
 *
 * The tick rate is 18.2 Hz, so units/s = units * 182 / (ticks * 10) and
 * the k-rate is that over 1000.  Doing it in that order overflows: this
 * ran as "iters * 182 / (el * 10 * 1000)" and a machine roughly a
 * hundred times a 386SX pushes iters past 11.8 million, at which point
 * iters * 182 exceeds a 32-bit long and the result came back as the
 * fallback 1 kOps/s next to a memory score of 585 MB/s.  Dividing by
 * 1000 FIRST keeps the product small - a thousand-fold headroom - and
 * costs only sub-kilo-unit precision, which the display rounds away
 * anyway.  Returns at least 1 so a rate that really is below 1k does
 * not read as "skipped". */
static long krate(unsigned long units, long ticks)
{
    unsigned long k;

    if (ticks < 1L)
        ticks = 1L;
    k = (units / 1000UL) * 182UL / ((unsigned long)ticks * 10UL);
    return (k > 0UL) ? (long)k : 1L;
}

/* KB per second from a byte count and a tick span.  Same overflow trap
 * as krate(), one order of magnitude further out: 902 MB copied in a
 * run is already 1.6e8 once multiplied by 182, and a machine only a few
 * times faster would pass the 32-bit ceiling.  Convert to KB first. */
static long kbps(unsigned long bytes, long ticks)
{
    if (ticks < 1L)
        ticks = 1L;
    return (long)((bytes >> 10) / ((unsigned long)ticks * 10UL) * 182UL
                  + ((bytes >> 10) % ((unsigned long)ticks * 10UL)) * 182UL
                    / ((unsigned long)ticks * 10UL));
}

static long bench_cpu(void)
{
    unsigned long t0 = ui_ticks();
    unsigned long iters = 0UL;
    long el = 0;
    long a = 1, b = 7, c = 13;
    int i;

    do {
        for (i = 0; i < 500; i++) {
            a += b;
            b ^= c;
            c += (a >> 3);
            a = a * 3L + 1L;
        }
        g_isink += a + b + c;
        iters += 500UL;
        el = ticks_since(t0);
        prog_update(el, (long)(iters / 1000UL), "k iterations");
    } while (el < DUR_TICKS);
    return krate(iters, el);
}

static long bench_fpu(void)
{
    unsigned long t0 = ui_ticks();
    unsigned long flops = 0UL;
    long el = 0;
    double x = 1.2345, y = 5.6789;
    int i;

    do {
        for (i = 0; i < 100; i++) {
            x = x * y + 0.5;
            y = y / x + 1.25;
        }
        g_dsink += x + y;
        flops += 400UL;         /* ~4 FLOPs per inner pair */
        el = ticks_since(t0);
        prog_update(el, (long)(flops / 1000UL), "kFLOP");
    } while (el < DUR_TICKS);
    return krate(flops, el);
}

static char membuf_a[16384];
static char membuf_b[16384];

static long bench_mem(void)
{
    unsigned long t0 = ui_ticks();
    long el = 0;
    unsigned long bytes = 0;

    do {
        memcpy(membuf_b, membuf_a, sizeof(membuf_a));
        memcpy(membuf_a, membuf_b, sizeof(membuf_b));
        bytes += 32768UL;
        el = ticks_since(t0);
        prog_update(el, (long)(bytes >> 10), "KB copied");
    } while (el < DUR_TICKS);
    if (el < 1) el = 1;
    return kbps(bytes, el);
}

/* The video benchmark's workload, defined HERE and nowhere else.
 *
 * It used to call ui_fill().  That made the score a measurement of two
 * things at once - the machine's video bandwidth AND whatever the shared
 * toolkit's fill routine happened to look like that release - so tuning
 * UI.C silently moved every machine's score and quietly invalidated
 * bench_base[3], the 20.1 screens/s measured on the reference 386SX.  It
 * duly did: UI.C now writes cell WORDS, which is roughly twice the fill
 * rate for the same machine.
 *
 * So the benchmark carries its own copy of the workload, frozen: one
 * byte-at-a-time pass over the 4000 bytes of B800:0000, which is what
 * the anchor was measured against.  Scores stay comparable across
 * releases, and the toolkit is free to get faster.  Do not "simplify"
 * this back into a ui_fill() call without re-measuring the anchor on the
 * reference machine.  The cells are volatile so that no compiler can
 * quietly fuse the two byte stores back into the word store this exists
 * to avoid measuring. */
static void vid_fill_screen(char ch, unsigned char attr)
{
    volatile unsigned char far *vram =
        (volatile unsigned char far *)MK_FP(0xB800, 0x0000);
    unsigned offset = 0;
    int i;

    for (i = 0; i < SCR_W * SCR_H; i++) {
        vram[offset]     = (unsigned char)ch;
        vram[offset + 1] = attr;
        offset += 2;
    }
}

static long bench_vid(void)
{
    unsigned long t0 = ui_ticks();
    long el = 0, fills = 0;

    do {
        vid_fill_screen((char)((fills & 1) ? 0xB1 : 0xB0),
                        (fills & 1) ? A_DESKTOP : UI_ATTR(C_DGRAY, C_BLUE));
        fills++;
        el = ticks_since(t0);
        if ((fills & 3) == 0)
            prog_update(el, fills, "screen fills");
    } while (el < DUR_TICKS);
    if (el < 1) el = 1;
    return (fills * 182L) / el;         /* screens per second, x10 */
}

#define DSK_FILE  "CMARK$$.TMP"
#define DSK_BLK   2048
#define DSK_BLKS  24                    /* 48 KB test file */

static char dskbuf[DSK_BLK];

static long bench_dsk(void)
{
    FILE *fp;
    unsigned long t0;
    long el = 0;
    unsigned long bytes = 0;
    size_t n;
    int i;

    /* Prepare the test file (excluded from the timing). */
    for (i = 0; i < DSK_BLK; i++)
        dskbuf[i] = (char)(i & 0xFF);
    fp = fopen(DSK_FILE, "wb");
    if (fp == NULL)
        return -1;
    for (i = 0; i < DSK_BLKS; i++) {
        if (fwrite(dskbuf, 1, DSK_BLK, fp) != DSK_BLK) {
            fclose(fp);
            remove(DSK_FILE);
            return -1;
        }
    }
    fclose(fp);

    t0 = ui_ticks();
    do {
        fp = fopen(DSK_FILE, "rb");
        if (fp == NULL)
            break;
        while ((n = fread(dskbuf, 1, DSK_BLK, fp)) > 0)
            bytes += (unsigned long)n;
        fclose(fp);
        el = ticks_since(t0);
        prog_update(el, (long)(bytes >> 10), "KB read");
    } while (el < DUR_TICKS);
    remove(DSK_FILE);
    if (el < 1) el = 1;
    if (bytes == 0)
        return -1;
    return kbps(bytes, el);
}

/* --- Run / save / load ------------------------------------------------ */

static void run_all(void)
{
    int i;
    for (i = 0; i < NBENCH; i++) {
        prog_begin(i);
        switch (i) {
        case 0: results[i] = bench_cpu(); break;
        case 1: results[i] = g_fpu ? bench_fpu() : -1; break;
        case 2: results[i] = bench_mem(); break;
        case 3:
            results[i] = bench_vid();
            draw_screen();              /* the video test paints over us */
            prog_begin(i);
            break;
        case 4: results[i] = bench_dsk(); break;
        }
        draw_mark(i);
    }
    have_run = 1;
    ui_fill(2, RUN_Y + 1, 76, 5, ' ', A_DESKTOP);
    ui_puts(BAR_X, RUN_Y + 1, "Suite complete.", UI_ATTR(C_LGREEN, C_BLUE));
    ui_puts(BAR_X, RUN_Y + 2,
            "Press S to save these scores as the new reference.", A_ITEM);
    draw_index();
}

static const char *score_paths[] = {
    "C:\\CASTALIA\\CFG\\CASTMARK.SCR",
    "CASTMARK.SCR"
};
#define NSCOREP (int)(sizeof(score_paths) / sizeof(score_paths[0]))
static const char *score_keys[NBENCH] = { "cpu", "fpu", "mem", "vid", "dsk" };

static void load_prev(void)
{
    int i, p;
    for (i = 0; i < NBENCH; i++)
        prev[i] = -1;
    for (p = 0; p < NSCOREP; p++) {
        if (ini_open(score_paths[p]) == INI_OK) {
            for (i = 0; i < NBENCH; i++)
                prev[i] = ini_get_int("last", score_keys[i], -1);
            prev_index = ini_get_int("last", "index", -1);
            return;
        }
    }
}

static int save_scores(void)
{
    FILE *fp = NULL;
    int p, i;

    for (p = 0; p < NSCOREP; p++) {
        fp = fopen(score_paths[p], "w");
        if (fp != NULL)
            break;
    }
    if (fp == NULL)
        return -1;
    fprintf(fp, "; CASTALIA MARK saved scores\r\n[last]\r\n");
    for (i = 0; i < NBENCH; i++)
        fprintf(fp, "%s=%ld\r\n", score_keys[i], results[i]);
    fprintf(fp, "index=%ld\r\n", castalia_index());
    fclose(fp);
    return 0;
}

/* --- Main -------------------------------------------------------------- */

int main(void)
{
    int i, key;

    for (i = 0; i < NBENCH; i++)
        results[i] = 0;
    g_fpu = fpu_present();
    load_prev();

    ui_init();
    draw_screen();

    for (;;) {
        key = ui_getkey();
        if (key == KEY_ESC)
            break;
        else if (key == KEY_ENTER) {
            run_all();
        } else if (key == 's' || key == 'S') {
            if (!have_run) {
                ui_puts(BAR_X, RUN_Y + 1,
                        "Run the suite first (Enter).            ", A_HINT);
            } else if (save_scores() == 0) {
                ui_puts(BAR_X, RUN_Y + 1,
                        "Scores saved as the new reference.      ",
                        UI_ATTR(C_LGREEN, C_BLUE));
            } else {
                ui_puts(BAR_X, RUN_Y + 1,
                        "Could not write CASTMARK.SCR.           ", A_WARN);
            }
        }
    }

    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();
    return 0;
}
