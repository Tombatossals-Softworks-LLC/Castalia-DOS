/* ===================================================================
 * SMOKE.C  -  DOS-side smoke test  (SMOKE.EXE, CI only)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Built with Open Watcom and executed INSIDE DOS (headless DOSBox in
 * CI): proves that the 16-bit build of the shared INI module parses
 * the real shipped configuration files under a real DOS filesystem.
 * Results go to TESTOUT.TXT as PASS/FAIL lines; the exit code is the
 * failure count, and the host asserts both.
 *
 * Deliberately console-only (no UI library): it must run unattended.
 *
 * Build:  wmake smoke      Run (CI):  scripts/test-dos.sh
 * C89 only.  No dynamic allocation.
 * =================================================================== */

#include <stdio.h>
#include <string.h>
#include "../../src/common/INI.H"

static FILE *out;
static int failures = 0;
static int checks = 0;

static void report(int ok, const char *name)
{
    checks++;
    if (!ok)
        failures++;
    fprintf(out, "%s %s\r\n", ok ? "PASS" : "FAIL", name);
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
}

static void str_is(const char *got, const char *want, const char *name)
{
    report(got != NULL && strcmp(got, want) == 0, name);
}

int main(void)
{
    out = fopen("TESTOUT.TXT", "w");
    if (out == NULL) {
        printf("cannot create TESTOUT.TXT\n");
        return 99;
    }

    /* --- GAMES.INI ---------------------------------------------------- */
    if (ini_open("config\\GAMES.INI") == INI_OK) {
        report(1, "GAMES.INI opens under DOS");
        report(ini_section_count() >= 10, "GAMES.INI has 10+ games");
        str_is(ini_get("WOLF3D", "exe"), "WOLF3D.EXE",
               "WOLF3D exe under DOS");
        str_is(ini_get_def("MONKEY", "profile", "?"), "CLEAN",
               "MONKEY profile is CLEAN");
        report(ini_get_bool("7GUEST", "requires_cd", 0) == 1,
               "7GUEST requires CD");
        report(ini_get_bool("WOLF3D", "requires_cd", 1) == 0,
               "WOLF3D does not require CD");
    } else {
        report(0, "GAMES.INI opens under DOS");
    }

    /* --- PROFILES.INI --------------------------------------------------- */
    if (ini_open("config\\PROFILES.INI") == INI_OK) {
        report(1, "PROFILES.INI opens under DOS");
        report(ini_get("CLEAN", "name") != NULL, "CLEAN profile present");
        report(ini_get("XMS", "name") != NULL, "XMS profile present");
        report(ini_get("SAFE", "name") != NULL, "SAFE profile present");
        report(ini_get_int("XMS", "conv_kb", 0) > 500,
               "XMS conv_kb target sane");
    } else {
        report(0, "PROFILES.INI opens under DOS");
    }

    /* --- CASTALIA.INI ----------------------------------------------------- */
    if (ini_open("config\\CASTALIA.INI") == INI_OK) {
        report(1, "CASTALIA.INI opens under DOS");
        str_is(ini_get_def("about", "codename", "?"), "Tombatossals",
               "codename is Tombatossals");
        str_is(ini_get_def("launcher", "default_profile", "?"), "XMS",
               "launcher default profile");
    } else {
        report(0, "CASTALIA.INI opens under DOS");
    }

    /* --- HELP.IDX ------------------------------------------------------------ */
    if (ini_open("help\\HELP.IDX") == INI_OK) {
        int i, ok = 1, n = ini_section_count();
        report(n >= 12, "HELP.IDX has 12+ topics");
        for (i = 0; i < n; i++) {
            const char *sec = ini_section_name(i);
            if (sec == NULL || ini_get(sec, "title") == NULL ||
                ini_get(sec, "file") == NULL) {
                ok = 0;
                break;
            }
        }
        report(ok, "every HELP.IDX topic has title and file");
    } else {
        report(0, "HELP.IDX opens under DOS");
    }

    fprintf(out, "RESULT %d checks %d failures\r\n", checks, failures);
    printf("RESULT %d checks %d failures\n", checks, failures);
    fclose(out);
    return failures;
}
