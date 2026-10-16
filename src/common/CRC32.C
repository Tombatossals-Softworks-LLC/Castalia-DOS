/* ===================================================================
 * CRC32.C  -  CRC-32 for CASTALIA DOS (see CRC32.H)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 * =================================================================== */

#include <stdio.h>
#include "CRC32.H"

static unsigned long crc_table[256];
static int crc_ready = 0;

static void crc_build(void)
{
    unsigned long c;
    int n, k;

    for (n = 0; n < 256; n++) {
        c = (unsigned long)n;
        for (k = 0; k < 8; k++)
            c = (c & 1UL) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        crc_table[n] = c;
    }
    crc_ready = 1;
}

unsigned long crc32_update(unsigned long crc, const unsigned char *buf,
                           unsigned len)
{
    if (!crc_ready)
        crc_build();
    while (len--) {
        crc = crc_table[(unsigned)((crc ^ *buf++) & 0xFFUL)] ^ (crc >> 8);
    }
    /* 'unsigned long' is 64 bits on the host; keep the value 32-bit so
     * the host tests and the DOS build agree bit for bit. */
    return crc & 0xFFFFFFFFUL;
}

unsigned long crc32_final(unsigned long crc)
{
    return (crc ^ 0xFFFFFFFFUL) & 0xFFFFFFFFUL;
}

int crc32_file(const char *path, unsigned long *crc, unsigned long *size,
               crc32_progress progress)
{
    /* Static: 4 KB on a 16-bit stack is a quarter of what DOS gives a
     * program, and this runs once at a time. */
    static unsigned char buf[4096];
    FILE *fp;
    unsigned long c = CRC32_INIT, total = 0, next = 0;
    size_t n;

    fp = fopen(path, "rb");
    if (fp == NULL)
        return -1;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        c = crc32_update(c, buf, (unsigned)n);
        total += (unsigned long)n;
        if (progress != NULL && total >= next) {
            progress(total);
            next = total + 65536UL;
        }
    }
    if (ferror(fp)) {
        fclose(fp);
        return -2;
    }
    fclose(fp);
    if (progress != NULL)
        progress(total);
    *crc = crc32_final(c);
    *size = total;
    return 0;
}
