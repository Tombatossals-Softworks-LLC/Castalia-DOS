/* ===================================================================
 * test_ini.c  -  host unit tests for src/common/INI.C
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * The INI module is pure C89, so the REAL DOS source compiles and runs
 * natively on the build host.  These tests pin down the parsing
 * behaviour every Castalia tool depends on (GAMES.INI, PROFILES.INI,
 * CASTALIA.INI, HELP.IDX).
 *
 * Build & run (see scripts/test-unit.sh):
 *   gcc -std=c89 -Wall -Wextra -Werror -Isrc/common \
 *       -o test_ini tests/unit/test_ini.c src/common/INI.C && ./test_ini
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "INI.H"

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

static const char *TMP = "test_ini.tmp";

static void write_file(const char *content)
{
    FILE *fp = fopen(TMP, "wb");
    if (fp == NULL) {
        printf("  FAIL cannot create %s\n", TMP);
        exit(1);
    }
    fwrite(content, 1, strlen(content), fp);
    fclose(fp);
}

static void streq(const char *got, const char *want, const char *name)
{
    checks++;
    if (got != NULL && strcmp(got, want) == 0) {
        printf("  PASS %s\n", name);
    } else {
        printf("  FAIL %s  (got \"%s\", want \"%s\")\n",
               name, got ? got : "(null)", want);
        failures++;
    }
}

int main(void)
{
    printf("== INI module unit tests ==\n");

    /* --- missing file ------------------------------------------------ */
    CHECK(ini_open("no_such_file.ini") == INI_ERR_OPEN,
          "missing file returns INI_ERR_OPEN");

    /* --- basic parse -------------------------------------------------- */
    write_file(
        "; comment line\n"
        "# another comment\n"
        "[alpha]\n"
        "key1 = value one\n"
        "key2=compact\n"
        "  key3   =   spaced   \n"
        "\n"
        "[Beta]\n"
        "num = 42\n"
        "neg = -7\n"
        "flag_yes = yes\n"
        "flag_no = No\n"
        "flag_on = ON\n"
        "flag_true = true\n"
        "flag_one = 1\n"
        "flag_zero = 0\n"
        "flag_junk = maybe\n"
        "empty =\n");

    CHECK(ini_open(TMP) == INI_OK, "well-formed file opens");
    CHECK(ini_section_count() == 2, "two sections found");
    streq(ini_section_name(0), "alpha", "section 0 name");
    streq(ini_section_name(1), "Beta", "section 1 name");
    CHECK(ini_section_name(2) == NULL, "out-of-range section is NULL");

    streq(ini_get("alpha", "key1"), "value one",
          "value with internal space");
    streq(ini_get("alpha", "key2"), "compact", "compact key=value");
    streq(ini_get("alpha", "key3"), "spaced",
          "whitespace trimmed around key and value");

    /* --- case-insensitivity ------------------------------------------ */
    streq(ini_get("ALPHA", "KEY1"), "value one",
          "section and key lookups are case-insensitive");
    streq(ini_get("beta", "NUM"), "42", "mixed-case section matches");

    /* --- misses and defaults ------------------------------------------ */
    CHECK(ini_get("alpha", "nope") == NULL, "missing key is NULL");
    CHECK(ini_get("gamma", "key1") == NULL, "missing section is NULL");
    streq(ini_get_def("alpha", "nope", "dflt"), "dflt",
          "ini_get_def returns default on miss");
    streq(ini_get_def("beta", "empty", "dflt"), "dflt",
          "ini_get_def treats empty value as default");

    /* --- ints and bools ------------------------------------------------ */
    CHECK(ini_get_int("beta", "num", -1) == 42, "int value parses");
    CHECK(ini_get_int("beta", "neg", 0) == -7, "negative int parses");
    CHECK(ini_get_int("beta", "nope", 99) == 99, "int default on miss");
    CHECK(ini_get_bool("beta", "flag_yes", 0) == 1, "bool yes");
    CHECK(ini_get_bool("beta", "flag_no", 1) == 0, "bool No");
    CHECK(ini_get_bool("beta", "flag_on", 0) == 1, "bool ON");
    CHECK(ini_get_bool("beta", "flag_true", 0) == 1, "bool true");
    CHECK(ini_get_bool("beta", "flag_one", 0) == 1, "bool 1");
    CHECK(ini_get_bool("beta", "flag_zero", 1) == 0, "bool 0");
    CHECK(ini_get_bool("beta", "flag_junk", 7) == 7,
          "unrecognised bool falls to default");

    /* --- CRLF (DOS) line endings --------------------------------------- */
    write_file("[dos]\r\nkey = crlf value\r\nnum=5\r\n");
    CHECK(ini_open(TMP) == INI_OK, "CRLF file opens");
    streq(ini_get("dos", "key"), "crlf value", "CRLF value parses clean");
    CHECK(ini_get_int("dos", "num", -1) == 5, "CRLF int parses");

    /* --- reload replaces state ------------------------------------------ */
    CHECK(ini_section_count() == 1, "reload replaced previous sections");
    CHECK(ini_get("alpha", "key1") == NULL,
          "old sections gone after reload");

    /* --- oversized file rejected ----------------------------------------- */
    {
        FILE *fp = fopen(TMP, "wb");
        long i;
        fprintf(fp, "[big]\n");
        for (i = 0; i < 4000; i++)
            fprintf(fp, "k%ld = value\n", i);      /* >> INI_MAX_SIZE */
        fclose(fp);
        CHECK(ini_open(TMP) == INI_ERR_TOOBIG,
              "oversized file returns INI_ERR_TOOBIG");
    }

    /* --- real repo configs parse (when run from the repo root) ---------- */
    if (ini_open("config/GAMES.INI") == INI_OK) {
        CHECK(ini_section_count() >= 10, "GAMES.INI has 10+ games");
        streq(ini_get("WOLF3D", "exe"), "WOLF3D.EXE",
              "GAMES.INI WOLF3D exe");
        streq(ini_get_def("WOLF3D", "profile", "?"), "XMS",
              "GAMES.INI WOLF3D profile");
        CHECK(ini_get_bool("7GUEST", "requires_cd", 0) == 1,
              "GAMES.INI 7GUEST requires CD");
        CHECK(ini_open("config/PROFILES.INI") == INI_OK,
              "PROFILES.INI opens");
        CHECK(ini_get_int("XMS", "conv_kb", 0) > 500,
              "PROFILES.INI XMS conv_kb sane");
        CHECK(ini_open("config/CASTALIA.INI") == INI_OK,
              "CASTALIA.INI opens");
        streq(ini_get_def("about", "codename", "?"), "Tombatossals",
              "CASTALIA.INI codename");
        /* Regression: HELP.IDX must keep keys on their own lines - the
         * parser treats a [section] line purely as a header, so inline
         * keys would silently vanish and empty HELP.EXE's index. */
        CHECK(ini_open("help/HELP.IDX") == INI_OK, "HELP.IDX opens");
        CHECK(ini_section_count() >= 12, "HELP.IDX has 12+ topics");
        {
            int i, ok = 1;
            for (i = 0; i < ini_section_count(); i++) {
                const char *sec = ini_section_name(i);
                if (sec == NULL || ini_get(sec, "title") == NULL ||
                    ini_get(sec, "file") == NULL) {
                    ok = 0;
                    break;
                }
            }
            CHECK(ok, "every HELP.IDX topic has title and file keys");
        }
    } else {
        printf("  (skip) repo config checks: not run from the repo root\n");
    }

    remove(TMP);
    printf("== %d checks, %d failure(s) ==\n", checks, failures);
    return (failures == 0) ? 0 : 1;
}
