/* ===================================================================
 * CATIDX.C  -  The online game catalogue's index format (see CATIDX.H)
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 * =================================================================== */

#include <string.h>
#include "CATIDX.H"

static int is_blank(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static char *trim(char *s)
{
    char *end;
    while (*s && is_blank(*s))
        s++;
    end = s + strlen(s);
    while (end > s && is_blank(end[-1]))
        *--end = '\0';
    return s;
}

static int is_upper_alnum(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

static char up(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}

static int same_ci(const char *a, const char *b)
{
    while (*a && *b) {
        if (up(*a) != up(*b))
            return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

/* Copy 'src' into a buffer of 'max' characters plus the terminator. */
static int copy_max(char *dst, const char *src, int max)
{
    int n = (int)strlen(src);
    if (n > max)
        return -1;
    memcpy(dst, src, (unsigned)n + 1);
    return 0;
}

/* Printable ASCII only.  What the screen can draw in a cell is what
 * the catalogue may say: no control codes, no top-half bytes that the
 * code page would turn into box lines. */
static int plain_text(const char *s, int max, int may_be_empty)
{
    int n = 0;
    if (*s == '\0')
        return may_be_empty;
    for (; *s; s++, n++) {
        if ((unsigned char)*s < 0x20 || (unsigned char)*s > 0x7E)
            return 0;
    }
    return n <= max;
}

/* The names DOS keeps for devices.  A folder or a program called CON
 * or NUL cannot be created as a file, and opening it talks to the
 * device instead. */
static int dos_device(const char *base, int len)
{
    static const char *dev[] = { "CON", "PRN", "AUX", "NUL", "CLOCK$" };
    char tmp[9];
    int i;

    if (len <= 0 || len > 8)
        return 0;
    for (i = 0; i < len; i++)
        tmp[i] = up(base[i]);
    tmp[len] = '\0';
    for (i = 0; i < 5; i++)
        if (strcmp(tmp, dev[i]) == 0)
            return 1;
    if (len == 4 && tmp[3] >= '1' && tmp[3] <= '9' &&
        (memcmp(tmp, "COM", 3) == 0 || memcmp(tmp, "LPT", 3) == 0))
        return 1;
    return 0;
}

/* An 8-character name: A-Z, 0-9, '_' and '-'. */
static int name83(const char *s, int len)
{
    int i;
    if (len < 1 || len > 8)
        return 0;
    for (i = 0; i < len; i++)
        if (!is_upper_alnum(s[i]) && s[i] != '_' && s[i] != '-')
            return 0;
    return !dos_device(s, len);
}

static int valid_id(const char *s)
{
    return name83(s, (int)strlen(s));
}

static int valid_exe(const char *s)
{
    const char *dot = strchr(s, '.');
    if (dot == NULL || !name83(s, (int)(dot - s)))
        return 0;
    return strcmp(dot, ".EXE") == 0 || strcmp(dot, ".COM") == 0 ||
           strcmp(dot, ".BAT") == 0;
}

/* Strict unsigned decimal: digits only, no sign, no blanks inside, and
 * no wrap.  atol() would accept "12abc" and overflow silently. */
static int parse_ulong(const char *s, unsigned long *out)
{
    unsigned long v = 0;
    if (*s == '\0')
        return 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9')
            return 0;
        if (v > (0xFFFFFFFFUL - (unsigned long)(*s - '0')) / 10UL)
            return 0;
        v = v * 10UL + (unsigned long)(*s - '0');
    }
    *out = v;
    return 1;
}

static int parse_hex32(const char *s, unsigned long *out)
{
    unsigned long v = 0;
    int i;
    for (i = 0; i < 8; i++) {
        char c = up(s[i]);
        int d;
        if (c >= '0' && c <= '9')
            d = c - '0';
        else if (c >= 'A' && c <= 'F')
            d = c - 'A' + 10;
        else
            return 0;
        v = (v << 4) | (unsigned long)d;
    }
    if (s[8] != '\0')
        return 0;
    *out = v & 0xFFFFFFFFUL;
    return 1;
}

static int in_list(const char *s, const char **list, int n)
{
    int i;
    for (i = 0; i < n; i++)
        if (strcmp(s, list[i]) == 0)
            return 1;
    return 0;
}

/* http://host[:port]/path - nothing else.  HTGET speaks plain HTTP
 * only, so an https:// address could never be fetched; and since the
 * address is handed to HTGET as an argument, only characters a URL
 * needs are let through. */
static int valid_url(const char *s)
{
    const char *p;
    int host = 0;

    if (strlen(s) > CAT_URL_MAX)
        return 0;
    if (!(up(s[0]) == 'H' && up(s[1]) == 'T' && up(s[2]) == 'T' &&
          up(s[3]) == 'P' && s[4] == ':' && s[5] == '/' && s[6] == '/'))
        return 0;
    for (p = s + 7; *p && *p != '/' && *p != ':'; p++, host++) {
        char c = up(*p);
        if (!is_upper_alnum(c) && c != '.' && c != '-')
            return 0;
    }
    if (host == 0)
        return 0;
    if (*p == ':') {
        int digits = 0;
        for (p++; *p >= '0' && *p <= '9'; p++)
            digits++;
        if (digits < 1 || digits > 5)
            return 0;
    }
    if (*p != '/')
        return 0;
    for (; *p; p++) {
        char c = up(*p);
        if (is_upper_alnum(c))
            continue;
        if (strchr("-._~/%+=&?:@,", c) == NULL)
            return 0;
    }
    return 1;
}

int cat_header_ok(const char *line)
{
    size_t n = strlen(CAT_MAGIC);
    unsigned long v;
    char buf[16];
    int i = 0;

    if (strncmp(line, CAT_MAGIC, n) != 0 || line[n] != ' ')
        return 0;
    line += n + 1;
    while (line[i] >= '0' && line[i] <= '9' && i < 15) {
        buf[i] = line[i];
        i++;
    }
    buf[i] = '\0';
    line += i;
    while (*line && is_blank(*line))
        line++;
    if (*line != '\0' || !parse_ulong(buf, &v))
        return 0;
    return v == CAT_VERSION;
}

int cat_parse_line(char *line, CAT_ENTRY *e)
{
    static const char *profiles[] = { "CLEAN", "XMS", "EMS", "CDROM" };
    static const char *sounds[] =
        { "NONE", "SPKR", "ADLIB", "SB", "SBPRO", "SB16" };
    char *f[CAT_FIELDS];
    char *p, *t;
    int n = 0;
    unsigned long v;

    memset(e, 0, sizeof(*e));
    t = trim(line);
    if (*t == '\0' || *t == ';')
        return CAT_SKIP;

    /* Split on '|'.  One field too many is as wrong as one too few: a
     * '|' inside a name would otherwise shift every field after it. */
    f[n++] = t;
    for (p = t; *p; p++) {
        if (*p == '|') {
            *p = '\0';
            if (n == CAT_FIELDS)
                return CAT_ERR_FIELDS;
            f[n++] = p + 1;
        }
    }
    if (n != CAT_FIELDS)
        return CAT_ERR_FIELDS;
    for (n = 0; n < CAT_FIELDS; n++)
        f[n] = trim(f[n]);

#define FAIL(code) do { memset(e, 0, sizeof(*e)); return (code); } while (0)

    if (!valid_id(f[0]) || copy_max(e->id, f[0], CAT_ID_MAX) != 0)
        FAIL(CAT_ERR_ID);
    if (!plain_text(f[1], CAT_NAME_MAX, 0) || !plain_text(f[3], CAT_GENRE_MAX, 1) ||
        !plain_text(f[12], CAT_LIC_MAX, 0) || !plain_text(f[14], CAT_NOTES_MAX, 1))
        FAIL(CAT_ERR_TEXT);
    copy_max(e->name, f[1], CAT_NAME_MAX);
    copy_max(e->genre, f[3], CAT_GENRE_MAX);
    copy_max(e->license, f[12], CAT_LIC_MAX);
    copy_max(e->notes, f[14], CAT_NOTES_MAX);

    if (!parse_ulong(f[2], &v) || (v != 0 && (v < 1970 || v > 2099)))
        FAIL(CAT_ERR_NUMBER);
    e->year = (int)v;
    if (!parse_ulong(f[4], &v) ||
        (v != 86 && v != 286 && v != 386 && v != 486 && v != 586))
        FAIL(CAT_ERR_NUMBER);
    e->cpu = (int)v;
    if (!parse_ulong(f[5], &v) || v == 0 || v > CAT_SIZE_MAX)
        FAIL(CAT_ERR_NUMBER);
    e->size = v;
    /* FAT16's ceiling: nothing larger can be unpacked onto one drive. */
    if (!parse_ulong(f[6], &v) || v == 0 || v > 2097152UL)
        FAIL(CAT_ERR_NUMBER);
    e->disk_kb = v;

    if (!parse_hex32(f[7], &e->crc))
        FAIL(CAT_ERR_CRC);
    if (!valid_exe(f[8]) || copy_max(e->exe, f[8], CAT_EXE_MAX) != 0)
        FAIL(CAT_ERR_EXE);
    if (!in_list(f[9], profiles, 4) || !in_list(f[10], sounds, 6))
        FAIL(CAT_ERR_ENUM);
    copy_max(e->profile, f[9], CAT_PROF_MAX);
    copy_max(e->sound, f[10], CAT_SOUND_MAX);
    if (same_ci(f[11], "yes"))
        e->mouse = 1;
    else if (!same_ci(f[11], "no"))
        FAIL(CAT_ERR_ENUM);
    if (!valid_url(f[13]))
        FAIL(CAT_ERR_URL);
    copy_max(e->url, f[13], CAT_URL_MAX);

#undef FAIL
    return CAT_OK;
}

const char *cat_error_text(int code)
{
    switch (code) {
    case CAT_OK:         return "ok";
    case CAT_SKIP:       return "comment";
    case CAT_ERR_FIELDS: return "wrong number of fields";
    case CAT_ERR_ID:     return "bad id";
    case CAT_ERR_TEXT:   return "bad or too long text";
    case CAT_ERR_NUMBER: return "bad number";
    case CAT_ERR_CRC:    return "bad CRC-32";
    case CAT_ERR_EXE:    return "bad program name";
    case CAT_ERR_ENUM:   return "bad profile, sound or mouse";
    case CAT_ERR_URL:    return "bad address (http:// only)";
    case CAT_ERR_HEADER: return "not a Castalia catalogue";
    default:             return "unknown error";
    }
}
