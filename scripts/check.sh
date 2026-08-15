#!/usr/bin/env bash
# =====================================================================
#  check.sh  -  CASTALIA DOS repository gate
# ---------------------------------------------------------------------
#  Fast, self-contained checks that need only gcc + coreutils (shellcheck
#  is used if present).  This is what CI runs on every push, and what you
#  can run locally before committing.  It does NOT need Open Watcom: it
#  syntax-checks the DOS C sources with a host compiler using the tiny
#  stub headers in ci/stubs/ (a real build is a separate, Watcom-only
#  step - see the Makefile and scripts/build-floppy.sh).
#
#  Checks:
#    1. C89 syntax of every src/**/*.C (gcc -std=c89 -Wall -Wextra -Werror)
#    2. shell scripts: bash -n, and shellcheck if available
#    3. repo sanity: the eight boot profiles exist, no leftover placeholder
#       markers, the release codenames are present, help index resolves
#
#  Exit 0 if all pass, 1 otherwise.
# =====================================================================
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT" || exit 1

STUBS="ci/stubs"
fails=0
pass() { printf '  [ OK ] %s\n' "$*"; }
bad()  { printf '  [FAIL] %s\n' "$*"; fails=$((fails+1)); }
head() { printf '\n== %s ==\n' "$*"; }

# ---- 1. C89 syntax gate --------------------------------------------
head "C89 syntax (gcc, host stubs)"
if ! command -v gcc >/dev/null 2>&1; then
    bad "gcc not found (required for the C syntax gate)"
else
    while IFS= read -r f; do
        if gcc -x c -std=c89 -fsyntax-only -Wall -Wextra -Werror \
               -Wno-unused-parameter -Wno-unused-function \
               -Dfar= -Dnear= -I"$STUBS" -Isrc/common "$f" 2>/tmp/cc.$$; then
            pass "$f"
        else
            bad "$f"
            sed 's/^/         /' /tmp/cc.$$
        fi
    done < <(find src tests/dos -name '*.C' | sort)
    rm -f /tmp/cc.$$
fi

head "host unit tests compile (tests/unit)"
if gcc -x c -std=c89 -fsyntax-only -Wall -Wextra -Werror \
       -Isrc/common tests/unit/test_ini.c 2>/tmp/cc.$$; then
    pass "tests/unit/test_ini.c"
else
    bad "tests/unit/test_ini.c"
    sed 's/^/         /' /tmp/cc.$$
fi
rm -f /tmp/cc.$$

# ---- 2. shell scripts ----------------------------------------------
head "shell scripts"
for s in scripts/*.sh; do
    if bash -n "$s"; then pass "bash -n $s"; else bad "bash -n $s"; fi
done
if command -v shellcheck >/dev/null 2>&1; then
    for s in scripts/*.sh; do
        if shellcheck -e SC1090,SC2086 "$s"; then pass "shellcheck $s"
        else bad "shellcheck $s"; fi
    done
else
    printf '  [skip] shellcheck not installed\n'
fi

# ---- 3. repo sanity -------------------------------------------------
head "config/CONFIG.SYS uses FreeDOS multi-config syntax"
# The FreeDOS kernel has NO code for the MS-DOS [MENU]/MENUITEM/[BLOCK]
# scheme - grep the kernel source and you will not find "MENUITEM" at all.
# A CONFIG.SYS written that way boots every profile's drivers at once and
# never sets %CONFIG%.  It cost a hardware test run to find out, so the
# wrong syntax is a hard failure here.
if grep -qiE '^[[:space:]]*(MENUITEM[[:space:]]*=|\[(MENU|COMMON)\])' \
        config/CONFIG.SYS; then
    bad "CONFIG.SYS uses MS-DOS [MENU]/MENUITEM syntax; this kernel ignores it"
else
    pass "no MS-DOS [MENU]/MENUITEM syntax"
fi
if grep -qiE '^[[:space:]]*MENUCOLOR[[:space:]]*=' config/CONFIG.SYS; then
    pass "MENUCOLOR present (enables the highlight bar)"
else
    bad "CONFIG.SYS has no MENUCOLOR; the menu would render unhighlighted"
fi
if grep -qiE '^[[:space:]]*MENUDEFAULT[[:space:]]*=' config/CONFIG.SYS; then
    pass "MENUDEFAULT present (preselect + timeout)"
else
    bad "CONFIG.SYS has no MENUDEFAULT; the menu would wait forever"
fi
# MENUCOLOR clears the screen, so it has to precede the MENU lines or the
# highlight bar lands on the wrong rows.
mc=$(grep -niE '^[[:space:]]*MENUCOLOR' config/CONFIG.SYS | sed -n '1s/:.*//p')
m1=$(grep -niE '^[[:space:]]*MENU([[:space:]]|$)' config/CONFIG.SYS | sed -n '1s/:.*//p')
if [ -n "$mc" ] && [ -n "$m1" ] && [ "$mc" -lt "$m1" ]; then
    pass "MENUCOLOR precedes the MENU lines"
else
    bad "MENUCOLOR must come before the first MENU line (it clears the screen)"
fi

head "all eight menu entries are selectable and configured"
for n in 1 2 3 4 5 6 7 8; do
    if ! grep -qE "^[[:space:]]*MENU[[:space:]]+$n\." config/CONFIG.SYS; then
        bad "no 'MENU $n.' line - entry $n would not appear on the menu"
    elif ! grep -qE "^[0-9]*${n}[0-9]*\?" config/CONFIG.SYS; then
        bad "no '$n?' directive - entry $n would not be selectable"
    else
        pass "menu entry $n"
    fi
done

head "AUTOEXEC.BAT maps every menu digit to a profile and branches on it"
n=1
for p in CLEAN XMS EMS CDROM WIN3X DIAG SAFE PROMPT; do
    # The kernel exports %CONFIG% as the DIGIT, not a block name.
    if ! grep -qE "^IF \"%CONFIG%\"==\"$n\" SET CASTPROFILE=$p\$" \
            config/AUTOEXEC.BAT; then
        bad "AUTOEXEC.BAT does not map %CONFIG%=$n to $p"
    elif ! grep -q "^:$p" config/AUTOEXEC.BAT; then
        bad "AUTOEXEC.BAT missing :$p label"
    else
        pass "profile $n -> $p"
    fi
    n=$((n+1))
done

head "no redirection characters inside batch REM comments"
# FreeCOM parses redirection BEFORE it notices a line is a comment, so a
# REM that merely mentions ">" makes the shell try to redirect into a file.
# This shipped, and printed "Can not redirect output to file ']'" on boot.
remredir=0
while IFS= read -r f; do
    if grep -nE '^[[:space:]]*(REM|rem)([[:space:]]|$).*[<>|]' "$f"; then
        bad "$f: REM comment contains a redirection character"
        remredir=1
    fi
done < <(find config -name '*.BAT' -o -name '*.bat' | sort)
[ "$remredir" -eq 0 ] && pass "config/*.BAT clean"

head "no leftover placeholder markers"
# Scan docs/config/src (not scripts/, which legitimately name these markers).
if grep -rniE 'same as above|rest omitted|\bTODO\b|\bFIXME\b' \
        docs config src 2>/dev/null; then
    bad "placeholder markers found (see above)"
else
    pass "none found"
fi

head "release codenames present in docs"
for n in Almenara Morella Tombatossals; do
    if grep -rq "$n" docs; then pass "codename $n"
    else bad "codename $n missing from docs"; fi
done

head "help index files resolve"
if [ -f help/HELP.IDX ]; then
    miss=0
    # Only real entries (lines beginning with "["); ignore the comment line.
    while IFS= read -r file; do
        [ -f "help/$file" ] || { bad "help page missing: $file"; miss=1; }
    done < <(grep '^\[' help/HELP.IDX \
                | grep -oE 'file=[A-Z][A-Z0-9.]*' | sed 's/file=//')
    [ "$miss" -eq 0 ] && pass "all HELP.IDX pages exist"
else
    bad "help/HELP.IDX not found"
fi

# ---- summary --------------------------------------------------------
printf '\n'
if [ "$fails" -eq 0 ]; then
    printf 'ALL CHECKS PASSED\n'
    exit 0
fi
printf '%d CHECK(S) FAILED\n' "$fails"
exit 1
