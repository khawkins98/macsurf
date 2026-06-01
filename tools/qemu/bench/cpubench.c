/*
 * cpubench.c -- tiny CPU/FPU/memory benchmark for Mac OS 9 PPC.
 *
 * Purpose: get a "comparative megahertz" feel for the QEMU/TCG guest by running
 * the SAME compiled binary here and on a real G3/G4, then dividing the rates.
 * The ratio is a direct speed comparison; multiply by the real machine's clock
 * to get an effective-MHz figure. (Effective MHz is workload-dependent: integer
 * loops emulate far faster than transcendental FP, so expect a range.)
 *
 * Build (CodeWarrior 8): New Project -> "Mac OS C Stationery" -> a SIOUX console
 * target (Std C Console PPC). Drop this one file in, Make, Run. SIOUX gives you
 * the printf console window automatically. C89-clean, no toolbox UI needed.
 *
 * Timing uses Microseconds() (high-resolution on PPC). No threads, no yielding;
 * it just runs flat out, so quit other apps and DON'T run it during a build.
 *
 * Tune the *_ITERS counts if a phase finishes too fast (< ~1s) to time well.
 */

#include <stdio.h>
#include <math.h>
#include <Timer.h>      /* Microseconds, UnsignedWide */

#define INT_ITERS   50000000L    /* integer crunch iterations            */
#define FP_ITERS     3000000L    /* transcendental FP iterations         */
#define MEM_WORDS      65536L    /* 64K longs = 256 KB working set       */
#define MEM_PASSES       400L    /* streaming passes over the buffer     */

/* UnsignedWide -> microseconds as a double */
static double usec_now(void)
{
    UnsignedWide w;
    Microseconds(&w);
    return ((double) w.hi) * 4294967296.0 + ((double) w.lo);
}

int main(void)
{
    static long tbl[1024];
    static long buf[MEM_WORDS];
    volatile long  isink = 0;
    volatile double fsink = 0.0;
    volatile long  msink = 0;
    long i, k, pass;
    unsigned long acc;
    double x, y;
    double t0, t1, secs;
    double int_rate, fp_rate, mem_mbps;

    printf("MacSurf cpubench -- OS 9 PPC\n");
    printf("(run the same binary here and on a real G3/G4; divide the rates)\n\n");

    for (i = 0; i < 1024; i++) tbl[i] = (i * 2654435761UL) & 0x7fffffffL;

    /* ---- integer ---- */
    acc = 12345UL;
    isink = 0;
    t0 = usec_now();
    for (i = 0; i < INT_ITERS; i++) {
        acc = acc * 1103515245UL + 12345UL;
        acc ^= acc >> 7;
        isink += tbl[acc & 1023];
    }
    t1 = usec_now();
    secs = (t1 - t0) / 1000000.0;
    int_rate = (secs > 0.0) ? ((double) INT_ITERS / secs / 1000000.0) : 0.0;
    printf("integer : %8.2f s   %8.2f M iters/s   (sink=%ld)\n",
           secs, int_rate, (long) isink);

    /* ---- floating point (transcendental) ---- */
    x = 1.0; y = 0.0;
    t0 = usec_now();
    for (i = 0; i < FP_ITERS; i++) {
        x = sqrt(x * 1.0000001 + 1.0);
        y += sin(x) * cos(x);
        if (x > 1.0e9) x = 1.0;
    }
    t1 = usec_now();
    fsink = y;
    secs = (t1 - t0) / 1000000.0;
    fp_rate = (secs > 0.0) ? ((double) FP_ITERS / secs / 1000000.0) : 0.0;
    printf("float   : %8.2f s   %8.2f M iters/s   (sink=%.4f)\n",
           secs, fp_rate, (double) fsink);

    /* ---- memory streaming ---- */
    for (k = 0; k < MEM_WORDS; k++) buf[k] = k;
    msink = 0;
    t0 = usec_now();
    for (pass = 0; pass < MEM_PASSES; pass++) {
        long s = 0;
        for (k = 0; k < MEM_WORDS; k++) s += buf[k];
        msink += s;
        buf[pass & (MEM_WORDS - 1)] = msink; /* defeat hoisting */
    }
    t1 = usec_now();
    secs = (t1 - t0) / 1000000.0;
    mem_mbps = (secs > 0.0)
        ? (((double) MEM_WORDS * 4.0 * (double) MEM_PASSES) / secs / 1048576.0)
        : 0.0;
    printf("memory  : %8.2f s   %8.2f MB/s read    (sink=%ld)\n",
           secs, mem_mbps, (long) msink);

    printf("\nheadline: %.2f M int-iters/s -- use this for the cross-machine ratio\n",
           int_rate);
    printf("done. (press Return / close the console)\n");
    return 0;
}
