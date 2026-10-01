/* ===================================================================
 * test_safeio.c  -  host unit tests for src/common/SAFEIO.C
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * SAFEIO is what stands between a failed write and a truncated
 * CONFIG.SYS, so its promises are pinned down here with real files:
 * a copy or replace either lands complete or leaves the old file as it
 * was, no temporary file is left behind, and a file is never copied
 * onto itself.  Built with -DSAFEIO_TEST, which swaps the INT 21h
 * TRUENAME call for the textual comparison DOS falls back to.
 *
 * Build & run (see scripts/test-unit.sh):
 *   gcc -std=c89 -Wall -Wextra -Werror -DSAFEIO_TEST -Isrc/common \
 *       -o test_safeio tests/unit/test_safeio.c src/common/SAFEIO.C
 *   ./test_safeio <scratch directory>
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SAFEIO.H"

static int failures = 0;
static int checks = 0;

#define CHECK(cond, name)                                       \
    do {                                                        \
        checks++;                                               \
        if (cond) {                                             \
            printf("  PASS %s\n", name);                        \
        } else {                                                \
            printf("  FAIL %s  (line %d)\n", name, __LINE__);   \
            failures++;                                         \
        }                                                       \
    } while (0)

static char dir[256];

/* 'name' inside the scratch directory. */
static const char *at(const char *name, char *buf)
{
    sprintf(buf, "%s/%s", dir, name);
    return buf;
}

static void write_file(const char *path, const char *content)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        printf("  FAIL cannot create %s\n", path);
        exit(1);
    }
    fwrite(content, 1, strlen(content), fp);
    fclose(fp);
}

/* 1 if 'path' holds exactly 'want'. */
static int holds(const char *path, const char *want)
{
    char got[256];
    size_t n;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return 0;
    n = fread(got, 1, sizeof(got) - 1, fp);
    fclose(fp);
    got[n] = '\0';
    return strcmp(got, want) == 0;
}

int main(int argc, char *argv[])
{
    char a[300], b[300], t[300], tmp[SIO_PATH];
    char longp[SIO_PATH + 8];

    if (argc > 1 && strlen(argv[1]) < 200)
        strcpy(dir, argv[1]);
    else
        strcpy(dir, ".");

    printf("== SAFEIO tests (in %s) ==\n", dir);

    /* --- temporary names ------------------------------------------- */
    CHECK(sio_tmpname("C:\\CASTALIA\\CFG\\GAMES.INI", tmp) == SIO_OK &&
          strcmp(tmp, "C:\\CASTALIA\\CFG\\CASTNEW.$$$") == 0,
          "tmpname sits in the directory of the target");
    CHECK(sio_tmpname("A:GAMES.INI", tmp) == SIO_OK &&
          strcmp(tmp, "A:CASTNEW.$$$") == 0,
          "tmpname keeps a bare drive prefix");
    CHECK(sio_tmpname("GAMES.INI", tmp) == SIO_OK &&
          strcmp(tmp, "CASTNEW.$$$") == 0,
          "tmpname of a bare name is in the current directory");
    memset(longp, 'X', sizeof(longp) - 1);
    longp[sizeof(longp) - 1] = '\0';
    longp[sizeof(longp) - 4] = '\\';    /* long directory, short name */
    CHECK(sio_tmpname(longp, tmp) == SIO_ERR_NAME,
          "tmpname refuses a path that would overflow SIO_PATH");

    /* --- same-file detection --------------------------------------- */
    CHECK(sio_same("C:\\GAMES\\X.TXT", "c:/games/x.txt"),
          "same file regardless of case and slash direction");
    CHECK(!sio_same("C:\\GAMES\\X.TXT", "C:\\GAMES\\X.TX"),
          "different names are different files");

    /* --- sio_copy -------------------------------------------------- */
    write_file(at("SRC.TXT", a), "source data\r\n");
    remove(at("DST.TXT", b));
    CHECK(sio_copy(at("SRC.TXT", a), at("DST.TXT", b)) == SIO_OK &&
          holds(b, "source data\r\n"),
          "copy creates a new destination with the source bytes");
    CHECK(!sio_exists(at("CASTCPY.$$$", t)),
          "copy leaves no temporary file behind");

    write_file(at("DST.TXT", b), "old destination\r\n");
    CHECK(sio_copy(at("SRC.TXT", a), b) == SIO_OK &&
          holds(b, "source data\r\n"),
          "copy replaces an existing destination");
    CHECK(!sio_exists(at("CASTOLD.$$$", t)),
          "replace leaves no CASTOLD.$$$ behind");

    write_file(b, "keep me\r\n");
    CHECK(sio_copy(at("MISSING.TXT", a), b) == SIO_ERR_SRC &&
          holds(b, "keep me\r\n"),
          "a missing source leaves the destination untouched");

    CHECK(sio_copy(b, b) == SIO_ERR_SAME && holds(b, "keep me\r\n"),
          "copying a file onto itself is refused and harmless");

    /* --- sio_replace ----------------------------------------------- */
    write_file(at("CASTNEW.$$$", t), "new version\r\n");
    CHECK(sio_replace(t, b) == SIO_OK && holds(b, "new version\r\n") &&
          !sio_exists(t),
          "replace swaps the new file in and consumes the temporary");

    remove(at("FRESH.TXT", a));
    write_file(t, "first version\r\n");
    CHECK(sio_replace(t, a) == SIO_OK && holds(a, "first version\r\n"),
          "replace creates a file that did not exist");

    /* No temporary file: the swap fails half-way and must roll back. */
    remove(t);
    CHECK(sio_replace(t, b) == SIO_ERR_SWAP && holds(b, "new version\r\n"),
          "a failed swap puts the old file back");
    CHECK(!sio_exists(at("CASTOLD.$$$", t)),
          "a failed swap leaves no CASTOLD.$$$ behind");

    CHECK(sio_replace(b, b) == SIO_ERR_SAME && holds(b, "new version\r\n"),
          "replacing a file with itself is refused and harmless");

    /* --- sio_close ------------------------------------------------- */
    {
        FILE *fp = fopen(at("CLOSE.TXT", a), "wb");
        int ok = (fp != NULL && fputs("x", fp) >= 0 && sio_close(fp) == 0);
        CHECK(ok, "close reports success when every write landed");
        remove(a);
    }

    remove(at("SRC.TXT", a));
    remove(at("DST.TXT", a));
    remove(at("FRESH.TXT", a));
    printf("== %d checks, %d failure(s) ==\n", checks, failures);
    return (failures == 0) ? 0 : 1;
}
