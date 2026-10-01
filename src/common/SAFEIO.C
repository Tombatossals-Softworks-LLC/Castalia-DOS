/* ===================================================================
 * SAFEIO.C  -  Crash-safe file writing for CASTALIA DOS tools
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * See SAFEIO.H.  C89, Open Watcom + Turbo C.  Built with -DSAFEIO_TEST
 * by tests/unit/test_safeio.c, which has no INT 21h to call.
 * =================================================================== */

#include <stdio.h>
#include <string.h>
#ifndef SAFEIO_TEST
#include <dos.h>
#endif
#include "SAFEIO.H"

#define NEW_NAME "CASTNEW.$$$"      /* callers write new versions here  */
#define CPY_NAME "CASTCPY.$$$"      /* sio_copy() builds its copy here  */
#define OLD_NAME "CASTOLD.$$$"      /* the old file while it is swapped */

/* --- names ---------------------------------------------------------- */

/* 'name' in the directory of 'path' (everything up to the last '\', '/'
 * or ':'), so a rename between the two never crosses a drive. */
static int sibling(const char *path, const char *name, char *out)
{
    int i, cut = 0;

    for (i = 0; path[i]; i++)
        if (path[i] == '\\' || path[i] == '/' || path[i] == ':')
            cut = i + 1;
    if (cut + (int)strlen(name) + 1 > SIO_PATH)
        return SIO_ERR_NAME;
    memcpy(out, path, (size_t)cut);
    strcpy(out + cut, name);
    return SIO_OK;
}

static char fold(char c)
{
    if (c >= 'a' && c <= 'z') return (char)(c - 32);
    if (c == '/') return '\\';
    return c;
}

static int same_text(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (fold(*a) != fold(*b))
            return 0;
    return *a == *b;
}

/* Canonical form of a path into a 128-byte buffer.  0 on success. */
static int truename(const char *in, char *out)
{
#ifdef SAFEIO_TEST
    /* The host build has no DOS to ask; the unit test exercises the
     * textual comparison that DOS falls back to as well. */
    strncpy(out, in, 127);
    out[127] = '\0';
    return 0;
#else
    /* INT 21h AH=60h (DOS 3+): DS:SI = name, ES:DI = 128-byte buffer.
     * It resolves the drive, the current directory, "." and "..", and
     * upper-cases the result.  The name does not have to exist yet. */
    union REGS r;
    struct SREGS s;

    segread(&s);
    r.h.ah = 0x60;
    s.ds   = FP_SEG(in);
    r.x.si = FP_OFF(in);
    s.es   = FP_SEG(out);
    r.x.di = FP_OFF(out);
    int86x(0x21, &r, &r, &s);
    return r.x.cflag ? -1 : 0;
#endif
}

/* --- public --------------------------------------------------------- */

int sio_exists(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return 0;
    fclose(fp);
    return 1;
}

int sio_same(const char *a, const char *b)
{
    static char ta[128], tb[128];

    if (truename(a, ta) == 0 && truename(b, tb) == 0)
        return same_text(ta, tb);
    /* A name DOS cannot resolve (bad drive, no disk) will not open
     * either; compare the text so an obvious match is still caught. */
    return same_text(a, b);
}

int sio_tmpname(const char *final, char *out)
{
    return sibling(final, NEW_NAME, out);
}

int sio_close(FILE *fp)
{
    int bad = ferror(fp);
    if (fclose(fp) != 0)
        bad = 1;
    return bad ? -1 : 0;
}

int sio_replace(const char *tmp, const char *final)
{
    char old[SIO_PATH];

    if (sibling(final, OLD_NAME, old) != SIO_OK) {
        remove(tmp);
        return SIO_ERR_NAME;
    }
    /* Removing 'tmp' or the stale 'old' would delete 'final' itself
     * if the caller's file happens to carry one of our names. */
    if (sio_same(tmp, final) || sio_same(old, final))
        return SIO_ERR_SAME;

    if (!sio_exists(final)) {
        if (rename(tmp, final) != 0) {
            remove(tmp);
            return SIO_ERR_SWAP;
        }
        return SIO_OK;
    }

    /* DOS rename() refuses an existing target, so the old file steps
     * aside rather than being deleted: until the new one carries the
     * name, the old one can still be put back. */
    remove(old);                    /* leftover of an interrupted swap */
    if (rename(final, old) != 0) {
        remove(tmp);
        return SIO_ERR_SWAP;
    }
    if (rename(tmp, final) != 0) {
        rename(old, final);
        remove(tmp);
        return SIO_ERR_SWAP;
    }
    remove(old);
    return SIO_OK;
}

int sio_copy(const char *src, const char *dst)
{
    static char buf[2048];          /* static: off a small DOS stack */
    char tmp[SIO_PATH];
    FILE *in, *out;
    size_t n;
    int rc = SIO_OK;

    if (sibling(dst, CPY_NAME, tmp) != SIO_OK)
        return SIO_ERR_NAME;
    if (sio_same(src, dst) || sio_same(src, tmp))
        return SIO_ERR_SAME;

    in = fopen(src, "rb");
    if (in == NULL)
        return SIO_ERR_SRC;
    out = fopen(tmp, "wb");
    if (out == NULL) {
        fclose(in);
        return SIO_ERR_DST;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            rc = SIO_ERR_DST;
            break;
        }
    }
    if (rc == SIO_OK && ferror(in))
        rc = SIO_ERR_SRC;           /* a short read is not end of file */
    fclose(in);
    if (sio_close(out) != 0 && rc == SIO_OK)
        rc = SIO_ERR_DST;
    if (rc != SIO_OK) {
        remove(tmp);
        return rc;
    }
    return sio_replace(tmp, dst);
}
