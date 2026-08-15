/* ===================================================================
 * INI.C  -  Minimal INI-file reader for CASTALIA DOS tools
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * See INI.H for the interface and design notes.  Everything lives in a
 * single static buffer; no dynamic allocation.  C89, Open Watcom / Turbo
 * C friendly.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "INI.H"

/* --- Module state (static; one loaded file at a time) --------------- */
static char  ini_buf[INI_MAX_SIZE];
static int   ini_size = 0;

static char *ini_lines[INI_MAX_LINES];   /* pointers into ini_buf       */
static int   ini_nlines = 0;

static char *ini_sec_name[INI_MAX_SECTIONS]; /* section name pointers    */
static int   ini_sec_line[INI_MAX_SECTIONS]; /* line index of the header */
static int   ini_nsec = 0;

/* --- Small helpers -------------------------------------------------- */

/* ASCII lower-case, locale-independent. */
static char lc(char c)
{
    if (c >= 'A' && c <= 'Z')
        return (char)(c + 32);
    return c;
}

static int is_space(char c)
{
    return (c == ' ' || c == '\t');
}

/* Trim leading and trailing blanks in place; return the new start. */
static char *trim(char *s)
{
    char *end;
    while (*s && is_space(*s))
        s++;
    end = s + strlen(s);
    while (end > s && is_space(end[-1]))
        *--end = '\0';
    return s;
}

/* Case-insensitive full-string compare: 1 if equal. */
static int ci_equal(const char *a, const char *b)
{
    while (*a && *b) {
        if (lc(*a) != lc(*b))
            return 0;
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0');
}

/* Compare the first 'alen' chars of 'a' with all of 'b', case-insensitive.
 * True only if 'b' is exactly 'alen' chars long. */
static int ci_equal_n(const char *a, int alen, const char *b)
{
    int i;
    for (i = 0; i < alen; i++) {
        if (b[i] == '\0' || lc(a[i]) != lc(b[i]))
            return 0;
    }
    return (b[alen] == '\0');
}

/* Split the loaded buffer into trimmed, non-empty, non-comment lines. */
static void ini_split_lines(void)
{
    char *p = ini_buf;
    char *start;
    char *t;

    ini_nlines = 0;
    while (*p != '\0') {
        start = p;
        /* Find end of this physical line. */
        while (*p != '\0' && *p != '\n' && *p != '\r')
            p++;
        if (*p != '\0') {
            *p = '\0';           /* terminate the line */
            p++;
            /* Absorb a paired CR/LF or LF/CR. */
            if (*p == '\n' || *p == '\r')
                p++;
        }
        t = trim(start);
        if (t[0] == '\0' || t[0] == ';' || t[0] == '#')
            continue;            /* blank or comment */
        if (ini_nlines < INI_MAX_LINES)
            ini_lines[ini_nlines++] = t;
    }
}

/* Scan the logical lines for [sections]. */
static void ini_scan_sections(void)
{
    int i;
    char *line;
    char *rb;

    ini_nsec = 0;
    for (i = 0; i < ini_nlines; i++) {
        line = ini_lines[i];
        if (line[0] != '[')
            continue;
        rb = strchr(line, ']');
        if (rb == NULL)
            continue;
        *rb = '\0';              /* cut name out of "[name]..." */
        if (ini_nsec < INI_MAX_SECTIONS) {
            ini_sec_name[ini_nsec] = line + 1;
            ini_sec_line[ini_nsec] = i;
            ini_nsec++;
        }
    }
}

/* --- Public interface ----------------------------------------------- */

int ini_open(const char *path)
{
    FILE *fp;
    size_t n;
    int extra;

    ini_size = 0;
    ini_nlines = 0;
    ini_nsec = 0;
    ini_buf[0] = '\0';

    fp = fopen(path, "rb");
    if (fp == NULL)
        return INI_ERR_OPEN;

    n = fread(ini_buf, 1, (size_t)(INI_MAX_SIZE - 1), fp);
    if (ferror(fp)) {
        fclose(fp);
        return INI_ERR_READ;
    }
    /* If there is still data left, the file is larger than our buffer. */
    extra = fgetc(fp);
    fclose(fp);
    if (extra != EOF)
        return INI_ERR_TOOBIG;

    ini_buf[n] = '\0';
    ini_size = (int)n;

    ini_split_lines();
    ini_scan_sections();
    return INI_OK;
}

int ini_section_count(void)
{
    return ini_nsec;
}

const char *ini_section_name(int index)
{
    if (index < 0 || index >= ini_nsec)
        return NULL;
    return ini_sec_name[index];
}

const char *ini_get(const char *section, const char *key)
{
    int s, i, start, end;
    char *line, *eq, *v, *kend;
    int klen;

    /* Locate the section. */
    for (s = 0; s < ini_nsec; s++) {
        if (ci_equal(ini_sec_name[s], section))
            break;
    }
    if (s >= ini_nsec)
        return NULL;

    start = ini_sec_line[s] + 1;
    end   = (s + 1 < ini_nsec) ? ini_sec_line[s + 1] : ini_nlines;

    for (i = start; i < end; i++) {
        line = ini_lines[i];
        if (line[0] == '[')          /* safety: next section */
            break;
        eq = strchr(line, '=');
        if (eq == NULL)
            continue;
        /* Key length = up to '=' with trailing blanks removed. */
        kend = eq;
        while (kend > line && is_space(kend[-1]))
            kend--;
        klen = (int)(kend - line);
        if (!ci_equal_n(line, klen, key))
            continue;
        /* Value = after '=', trimmed. */
        v = eq + 1;
        while (*v && is_space(*v))
            v++;
        {
            char *e = v + strlen(v);
            while (e > v && is_space(e[-1]))
                *--e = '\0';
        }
        return v;
    }
    return NULL;
}

const char *ini_get_def(const char *section, const char *key,
                        const char *def)
{
    const char *v = ini_get(section, key);
    if (v == NULL || v[0] == '\0')
        return def;
    return v;
}

int ini_get_int(const char *section, const char *key, int def)
{
    const char *v = ini_get(section, key);
    if (v == NULL || v[0] == '\0')
        return def;
    return atoi(v);
}

int ini_get_bool(const char *section, const char *key, int def)
{
    const char *v = ini_get(section, key);
    if (v == NULL || v[0] == '\0')
        return def;
    if (ci_equal(v, "yes") || ci_equal(v, "on") ||
        ci_equal(v, "true") || ci_equal(v, "1"))
        return 1;
    if (ci_equal(v, "no") || ci_equal(v, "off") ||
        ci_equal(v, "false") || ci_equal(v, "0"))
        return 0;
    return def;
}
