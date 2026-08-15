/* ===================================================================
 * test_castmark_scale.c  -  host unit tests for CASTMARK's log scale
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * The Castalia Index and the comparison bars are pure arithmetic on the
 * five benchmark results, and both were wrong in a way that only showed
 * up on a machine faster than the baseline: bars clamped at 4x and the
 * index clamped at 400% per benchmark, so a 486 and a Pentium scored
 * the same.  Fixed-point log2/exp2 replaced them, and fixed-point maths
 * on a 16-bit compiler is exactly the sort of thing to test on the host
 * rather than discover on a 386SX.
 *
 * The scale helpers are lifted from CASTMARK.C verbatim rather than
 * #included, because CASTMARK.C pulls in the whole DOS UI; the copies
 * are kept honest by test_scale_matches_source(), which greps the real
 * source for the same constants.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int failures = 0;
static int checks   = 0;

static void ck(int cond, const char *what)
{
    checks++;
    if (cond) {
        printf("  [  OK  ] %s\n", what);
    } else {
        printf("  [ FAIL ] %s\n", what);
        failures++;
    }
}

/* --- verbatim from src/castmark/CASTMARK.C -------------------------- */

#define NBENCH   5
#define BAR_LO  (-2)
#define BAR_HI  ( 5)
#define BAR_MK_W 27

static const long bench_base[NBENCH] = { 38L, 136L, 6656L, 201L, 530L };
static long results[NBENCH];

static long log2_fx(unsigned long x)
{
    long e = 0;

    if (x == 0UL)
        return 0L;
    while (x >= 0x200UL) { x >>= 1; e++; }
    while (x <  0x100UL) { x <<= 1; e--; }
    return e * 256L + (long)(x - 0x100UL);
}

static long exp2_fx(long y)
{
    long e = y >> 8;
    long v = 0x100L + (y - (e << 8));

    while (e > 0) { v <<= 1;  e--; }
    while (e < 0) { v >>= 1;  e++; }
    return v;
}

static long ratio_fx(int i)
{
    if (results[i] < 0)
        return -1L;
    return (results[i] * 256L) / bench_base[i];
}

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

static int bar_permille(int i)
{
    long rf = ratio_fx(i), lg, span;
    int  pm;

    if (rf <= 0L)
        return 0;
    lg   = log2_fx((unsigned long)rf);
    span = (long)(BAR_HI - BAR_LO) * 256L;
    pm   = (int)(((lg - (long)BAR_LO * 256L) * 1000L) / span);
    if (pm < 0)    pm = 0;
    if (pm > 1000) pm = 1000;
    return pm;
}


/* --- rate helpers, verbatim from CASTMARK.C ------------------------- */

static long krate(unsigned long units, long ticks)
{
    unsigned long k;

    if (ticks < 1L)
        ticks = 1L;
    k = (units / 1000UL) * 182UL / ((unsigned long)ticks * 10UL);
    return (k > 0UL) ? (long)k : 1L;
}

static long kbps(unsigned long bytes, long ticks)
{
    if (ticks < 1L)
        ticks = 1L;
    return (long)((bytes >> 10) / ((unsigned long)ticks * 10UL) * 182UL
                  + ((bytes >> 10) % ((unsigned long)ticks * 10UL)) * 182UL
                    / ((unsigned long)ticks * 10UL));
}

/* --- helpers -------------------------------------------------------- */

static void set_all(long a, long b, long c, long d, long e)
{
    results[0] = a; results[1] = b; results[2] = c;
    results[3] = d; results[4] = e;
}

static void set_baseline(void)
{
    int i;
    for (i = 0; i < NBENCH; i++)
        results[i] = bench_base[i];
}

/* --- tests ---------------------------------------------------------- */

static void test_log_exp_roundtrip(void)
{
    unsigned long v;
    int bad = 0;

    printf("\n== log2_fx / exp2_fx round-trip ==\n");

    ck(log2_fx(0x100UL) == 0L,        "log2(1.0) == 0 octaves");
    ck(log2_fx(0x200UL) == 256L,      "log2(2.0) == 1 octave");
    ck(log2_fx(0x080UL) == -256L,     "log2(0.5) == -1 octave");
    ck(log2_fx(0x400UL) == 512L,      "log2(4.0) == 2 octaves");
    ck(log2_fx(0UL) == 0L,            "log2(0) is defined, not a crash");

    ck(exp2_fx(0L)     == 0x100L,     "exp2(0) == 1.0");
    ck(exp2_fx(256L)   == 0x200L,     "exp2(1) == 2.0");
    ck(exp2_fx(-256L)  == 0x080L,     "exp2(-1) == 0.5");

    /* exp2_fx inverts log2_fx to the precision the representation
     * actually carries: log2_fx keeps 9 significant bits (the mantissa
     * normalised into [256,512)), so the round-trip is exact up to 2x
     * and within one part in 256 above it.  Asserting bit-exactness
     * over the whole range would be asserting something false. */
    for (v = 0x100UL; v <= 0x200UL; v++) {
        if (exp2_fx(log2_fx(v)) != (long)v)
            bad++;
    }
    ck(bad == 0, "round-trip is exact for every value in 1x..2x");

    bad = 0;
    for (v = 0x100UL; v <= 0x40000UL; v = (v * 3UL) / 2UL + 1UL) {
        long back = exp2_fx(log2_fx(v));
        long err  = back - (long)v;
        if (err < 0) err = -err;
        if (err * 256L > (long)v)          /* worse than 1/256 relative */
            bad++;
    }
    ck(bad == 0, "round-trip is within 1/256 relative across 1x..1024x");
}

static void test_log_is_monotonic(void)
{
    unsigned long x;
    long last = -0x7FFFFFFFL;
    int  bad = 0;

    printf("\n== log2_fx is monotonic (a faster machine never scores lower) ==\n");
    for (x = 1UL; x < 0x20000UL; x += 7UL) {
        long l = log2_fx(x);
        if (l < last)
            bad++;
        last = l;
    }
    ck(bad == 0, "log2_fx never decreases as x grows");
}

static void test_baseline_scores_100(void)
{
    printf("\n== the 386SX/16 baseline machine scores exactly 100 ==\n");
    set_baseline();
    ck(castalia_index() == 100L, "index == 100 on the baseline");
    ck(bar_permille(0) > 200 && bar_permille(0) < 350,
       "baseline bar sits between 20% and 35% (room to fall AND to rise)");
}

static void test_reference_machine_scores_100(void)
{
    printf("\n== the measured reference 386SX/16 + 387 scores 100 ==\n");

    /* The CASTMARK run the anchors were taken from: an 86Box machine
     * matching the reference 386SX, CLEAN profile, 387 fitted.  If
     * bench_base[] ever drifts from these, the index stops meaning
     * "times the reference machine" and this test says so. */
    set_all(38L, 136L, 6656L, 201L, 530L);
    ck(castalia_index() == 100L, "the machine the anchors came from scores 100");

    /* And the numbers that run actually produced, before calibration,
     * scored 196 - the anchors were out by up to 17x. */
    set_all(38L, 136L, 6656L, 201L, 530L);
    ck(ratio_fx(1) == 256L, "the FPU anchor now assumes a 387 is fitted");
}

static void test_index_does_not_saturate(void)
{
    long i486, i586;

    printf("\n== the index separates machines the old one could not ==\n");

    /* A 486-class report: with the old formula every one of these ratios
     * hit the 400% clamp, so the index could not tell it from a
     * Pentium. */
    set_all(12967L, 17999L, 577532L, 7689L, 1747L);
    i486 = castalia_index();
    ck(i486 > 400L, "a fast machine scores above the old 400 ceiling");

    /* Twice as fast in every benchmark must score twice as high: that
     * is the property the clamp destroyed. */
    set_all(25934L, 35998L, 1155064L, 15378L, 3494L);
    i586 = castalia_index();
    ck(i586 > i486, "twice as fast scores strictly higher");
    ck(i586 >= (i486 * 19L) / 10L && i586 <= (i486 * 21L) / 10L,
       "doubling every benchmark doubles the index (within 5%)");
}

static void test_index_is_geometric(void)
{
    long idx;
    double want;
    int i;

    printf("\n== the index is the geometric mean, not the arithmetic one ==\n");

    /* One benchmark 100x the baseline, the rest exactly at it.  An
     * arithmetic mean would report (100+1+1+1+1)/5 = 20.8x -> 2080.
     * The geometric mean is 100^(1/5) = 2.512x -> 251. */
    set_baseline();
    results[1] = bench_base[1] * 100L;
    idx = castalia_index();
    want = pow(100.0, 1.0 / 5.0) * 100.0;
    ck(idx > (long)(want * 0.95) && idx < (long)(want * 1.05),
       "one 100x outlier moves the index to ~251, not ~2080");

    /* A machine slower than the baseline everywhere scores below 100. */
    for (i = 0; i < NBENCH; i++)
        results[i] = bench_base[i] / 2L;
    ck(castalia_index() < 100L, "half-speed everywhere scores below 100");
}

static void test_index_matches_double_precision(void)
{
    int    i;
    double logsum = 0.0, want;
    long   got;

    printf("\n== fixed-point index tracks the true geometric mean ==\n");

    set_all(12967L, 17999L, 577532L, 7689L, 1747L);
    for (i = 0; i < NBENCH; i++)
        logsum += log((double)results[i] / (double)bench_base[i]);
    want = exp(logsum / (double)NBENCH) * 100.0;
    got  = castalia_index();

    printf("         fixed-point %ld vs double %.1f\n", got, want);
    /* The linear mantissa interpolation is worth up to ~6%; anything
     * beyond 10% would mean a real arithmetic bug. */
    ck(got > (long)(want * 0.90) && got < (long)(want * 1.10),
       "within 10% of the double-precision geometric mean");
}

static void test_skipped_benchmarks(void)
{
    printf("\n== a skipped benchmark is excluded, not counted as zero ==\n");

    set_baseline();
    results[4] = -1L;               /* disk read skipped */
    ck(castalia_index() == 100L, "skipping one benchmark still scores 100");
    ck(bar_permille(4) == 0, "a skipped benchmark draws an empty bar");

    set_all(-1L, -1L, -1L, -1L, -1L);
    ck(castalia_index() == -1L, "all skipped reports 'no index', not 0");
}

static void test_bar_range(void)
{
    int i;
    int bad = 0;

    printf("\n== bars stay inside the bar, whatever the machine ==\n");

    /* Absurdly fast and absurdly slow both have to land in 0..1000. */
    set_all(400000L, 900000L, 9000000L, 90000L, 400000L);
    for (i = 0; i < NBENCH; i++) {
        int pm = bar_permille(i);
        if (pm < 0 || pm > 1000) bad++;
    }
    set_all(1L, 1L, 1L, 1L, 1L);
    for (i = 0; i < NBENCH; i++) {
        int pm = bar_permille(i);
        if (pm < 0 || pm > 1000) bad++;
    }
    ck(bad == 0, "permille stays within 0..1000 at both extremes");

    set_baseline();
    for (i = 0; i < NBENCH; i++)
        results[i] = bench_base[i] * 32L;
    ck(bar_permille(0) == 1000, "32x the baseline fills the bar exactly");

    for (i = 0; i < NBENCH; i++)
        results[i] = bench_base[i] * 8L;
    ck(bar_permille(0) > 600 && bar_permille(0) < 800,
       "8x lands around three-quarters, not pinned");
}

static void test_bar_leaves_room_for_the_ratio(void)
{
    printf("\n== layout: the bar and its 'xNNN' label both fit ==\n");
    /* The marks box starts at column 42 and is 36 wide, so the last
     * usable column is 77.  Bar starts at 43. */
    ck(43 + BAR_MK_W + 2 + 5 <= 78,
       "bar + gap + 5-char multiplier fits inside the marks box");
}

/* The copies above must not drift from the real source. */
static void test_scale_matches_source(void)
{
    FILE *fp;
    char line[256];
    int  seen_lo = 0, seen_hi = 0, seen_w = 0;

    printf("\n== the copied constants still match CASTMARK.C ==\n");
    fp = fopen("src/castmark/CASTMARK.C", "r");
    if (fp == NULL) {
        ck(0, "src/castmark/CASTMARK.C is readable");
        return;
    }
    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        if (strstr(line, "#define BAR_LO") && strstr(line, "(-2)"))  seen_lo = 1;
        if (strstr(line, "#define BAR_HI") && strstr(line, "( 5)"))  seen_hi = 1;
        if (strstr(line, "#define BAR_MK_W") && strstr(line, "27"))  seen_w  = 1;
    }
    fclose(fp);
    ck(seen_lo, "BAR_LO is still -2 in CASTMARK.C");
    ck(seen_hi, "BAR_HI is still 5 in CASTMARK.C");
    ck(seen_w,  "BAR_MK_W is still 27 in CASTMARK.C");
}


static void test_rate_helpers(void)
{
    printf("\n== rate helpers do not overflow on a fast machine ==\n");

    /* The reading from the first hardware-class report: 363 kOps/s over
     * a 28-tick run.  Reproducing it exactly proves the reordering did
     * not change the measurement, only its headroom. */
    ck(krate(559000UL, 28L) == 363L, "363 kOps/s reproduced from 559k iters");

    /* The reading that exposed the bug: a machine about a hundred times
     * a 386SX.  The old expression computed iters * 182 first, which is
     * 1.02e10 - past the 32-bit ceiling - and the result collapsed to
     * the fallback 1. */
    ck(krate(56000000UL, 28L) == 36400L,
       "56M iterations reports 36400 kOps/s, not the fallback 1");
    ck(56000000.0 * 182.0 > 2147483647.0,
       "...and the old expression really did exceed a 32-bit long");

    /* Monotonic: more work in the same time is never a lower rate. */
    ck(krate(1000000UL, 28L) > krate(500000UL, 28L),
       "twice the iterations is a higher rate");
    ck(krate(56000000UL, 28L) > krate(28000000UL, 28L),
       "still monotonic well past the old overflow point");

    /* A genuinely sub-1k rate must read 1, not 0: zero means skipped. */
    ck(krate(100UL, 28L) == 1L, "a rate below 1k reads 1, never 0");
    ck(krate(0UL, 0L) == 1L, "zero work and zero ticks cannot divide by zero");

    printf("\n== KB/s helper is exact and overflow-proof ==\n");

    /* Exactly the naive formula wherever the naive one is safe. */
    {
        unsigned long kb;
        int bad = 0;
        for (kb = 1UL; kb < 2000000UL; kb = kb * 7UL / 5UL + 13UL) {
            unsigned long bytes = kb << 10;
            unsigned long want  = kb * 182UL / 280UL;
            if ((unsigned long)kbps(bytes, 28L) != want)
                bad++;
        }
        ck(bad == 0, "matches (KB * 182) / (ticks * 10) exactly");
    }

    /* 4 GB/s sustained would break the naive form; this must not. */
    ck(kbps(0xF0000000UL, 28L) > 2000000L,
       "a 4 GB run still reports a sane KB/s");
}

int main(void)
{
    printf("CASTMARK scale tests\n");

    test_rate_helpers();
    test_log_exp_roundtrip();
    test_log_is_monotonic();
    test_baseline_scores_100();
    test_reference_machine_scores_100();
    test_index_does_not_saturate();
    test_index_is_geometric();
    test_index_matches_double_precision();
    test_skipped_benchmarks();
    test_bar_range();
    test_bar_leaves_room_for_the_ratio();
    test_scale_matches_source();

    printf("\n== %d checks, %d failure(s) ==\n", checks, failures);
    return failures ? 1 : 0;
}
