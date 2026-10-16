/* ===================================================================
 * test_catidx.c  -  host unit tests for src/common/CATIDX.C + CRC32.C
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Both modules are pure C89, so the REAL DOS sources run here.  The
 * index parser is the first thing in Castalia that reads data from the
 * network, so most of these tests are about what it refuses.
 *
 * Build & run (see scripts/test-unit.sh):
 *   gcc -std=c89 -Wall -Wextra -Werror -Isrc/common -o test_catidx \
 *       tests/unit/test_catidx.c src/common/CATIDX.C src/common/CRC32.C
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "CATIDX.H"
#include "CRC32.H"

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

#define GOOD "SNAKE|Castalia Snake|2026|Arcade|386|48213|96|1A2B3C4D|SNAKE.EXE|" \
             "CLEAN|SPKR|no|MIT|http://dos.castaliaos.com/games/SNAKE.ZIP|" \
             "The castle's own snake."

/* Parse a copy, so each test starts from the literal. */
static int parse(const char *text, CAT_ENTRY *e)
{
    static char buf[CAT_LINE_MAX + 64];
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    return cat_parse_line(buf, e);
}

/* GOOD with field 'idx' replaced by 'value'. */
static int parse_with(int idx, const char *value, CAT_ENTRY *e)
{
    char src[CAT_LINE_MAX + 64], out[CAT_LINE_MAX + 128];
    char *fields[CAT_FIELDS], *p;
    int n = 0, i;

    strcpy(src, GOOD);
    fields[n++] = src;
    for (p = src; *p; p++)
        if (*p == '|') { *p = '\0'; fields[n++] = p + 1; }
    out[0] = '\0';
    for (i = 0; i < CAT_FIELDS; i++) {
        if (i) strcat(out, "|");
        strcat(out, i == idx ? value : fields[i]);
    }
    return parse(out, e);
}

static void test_crc32(void)
{
    unsigned long c, size, crc;
    FILE *fp;
    static unsigned char big[100000];
    unsigned i;

    printf("== CRC-32 ==\n");
    c = crc32_update(CRC32_INIT, (const unsigned char *)"123456789", 9);
    CHECK(crc32_final(c) == 0xCBF43926UL, "standard check value of \"123456789\"");
    CHECK(crc32_final(crc32_update(CRC32_INIT, (const unsigned char *)"", 0)) == 0UL,
          "empty input is 0");

    /* Split updates agree with one pass. */
    c = crc32_update(CRC32_INIT, (const unsigned char *)"1234", 4);
    c = crc32_update(c, (const unsigned char *)"56789", 5);
    CHECK(crc32_final(c) == 0xCBF43926UL, "chunked update equals one pass");

    for (i = 0; i < sizeof(big); i++)
        big[i] = (unsigned char)((i * 7u + 3u) & 0xFFu);
    fp = fopen("test_crc.tmp", "wb");
    fwrite(big, 1, sizeof(big), fp);
    fclose(fp);
    CHECK(crc32_file("test_crc.tmp", &crc, &size, NULL) == 0, "crc32_file reads a file");
    CHECK(size == sizeof(big), "crc32_file reports the size");
    c = crc32_update(CRC32_INIT, big, (unsigned)sizeof(big));
    CHECK(crc == crc32_final(c), "crc32_file agrees with the in-memory CRC across 4 KB chunks");
    remove("test_crc.tmp");
    CHECK(crc32_file("no-such-file.tmp", &crc, &size, NULL) == -1, "a missing file is -1");
}

static void test_header(void)
{
    printf("== header ==\n");
    CHECK(cat_header_ok("CASTALIA-CATALOG 1"), "version 1 accepted");
    CHECK(cat_header_ok("CASTALIA-CATALOG 1\r"), "trailing CR accepted");
    CHECK(!cat_header_ok("CASTALIA-CATALOG 2"), "a newer version is refused, not misread");
    CHECK(!cat_header_ok("CASTALIA-CATALOG"), "no version is refused");
    CHECK(!cat_header_ok("<html><head>"), "an HTML error page is not a catalogue");
    CHECK(!cat_header_ok("CASTALIA-CATALOG 1x"), "junk after the version is refused");
}

static void test_good(void)
{
    CAT_ENTRY e;
    printf("== a good line ==\n");
    CHECK(parse(GOOD, &e) == CAT_OK, "parses");
    CHECK(strcmp(e.id, "SNAKE") == 0, "id");
    CHECK(strcmp(e.name, "Castalia Snake") == 0, "name");
    CHECK(e.year == 2026 && e.cpu == 386, "year and cpu");
    CHECK(e.size == 48213UL && e.disk_kb == 96UL, "sizes");
    CHECK(e.crc == 0x1A2B3C4DUL, "crc");
    CHECK(strcmp(e.exe, "SNAKE.EXE") == 0, "exe");
    CHECK(strcmp(e.profile, "CLEAN") == 0 && strcmp(e.sound, "SPKR") == 0, "profile and sound");
    CHECK(e.mouse == 0, "mouse");
    CHECK(strcmp(e.url, "http://dos.castaliaos.com/games/SNAKE.ZIP") == 0, "url");
    CHECK(strcmp(e.notes, "The castle's own snake.") == 0, "notes");

    CHECK(parse("  " GOOD "  \r\n", &e) == CAT_OK, "surrounding blanks and CRLF are fine");
    CHECK(parse_with(2, "0", &e) == CAT_OK && e.year == 0, "year 0 means not stated");
    CHECK(parse_with(14, "", &e) == CAT_OK, "notes may be empty");
    CHECK(parse_with(11, "YES", &e) == CAT_OK && e.mouse == 1, "mouse yes, any case");
    CHECK(parse_with(13, "http://10.0.2.2:8099/x/A.ZIP", &e) == CAT_OK, "an address with a port");
    CHECK(parse_with(7, "deadbeef", &e) == CAT_OK && e.crc == 0xDEADBEEFUL, "lower-case hex");
}

static void test_skip(void)
{
    CAT_ENTRY e;
    printf("== comments ==\n");
    CHECK(parse("", &e) == CAT_SKIP, "blank");
    CHECK(parse("   \r", &e) == CAT_SKIP, "blank with CR");
    CHECK(parse("; a comment", &e) == CAT_SKIP, "comment");
}

static void test_refused(void)
{
    CAT_ENTRY e;
    char longname[80];

    printf("== refused ==\n");
    CHECK(parse("SNAKE|too|few", &e) == CAT_ERR_FIELDS, "too few fields");
    CHECK(parse(GOOD "|extra", &e) == CAT_ERR_FIELDS, "a '|' too many shifts nothing: refused");

    CHECK(parse_with(0, "snake", &e) == CAT_ERR_ID, "lower-case id");
    CHECK(parse_with(0, "TOOLONGID", &e) == CAT_ERR_ID, "9-character id");
    CHECK(parse_with(0, "..", &e) == CAT_ERR_ID, "'..' as id (it names a folder)");
    CHECK(parse_with(0, "A\\B", &e) == CAT_ERR_ID, "backslash in id");
    CHECK(parse_with(0, "CON", &e) == CAT_ERR_ID, "device name CON");
    CHECK(parse_with(0, "LPT1", &e) == CAT_ERR_ID, "device name LPT1");
    CHECK(parse_with(0, "", &e) == CAT_ERR_ID, "empty id");

    memset(longname, 'A', 41);
    longname[41] = '\0';
    CHECK(parse_with(1, longname, &e) == CAT_ERR_TEXT, "41-character name");
    CHECK(parse_with(1, "", &e) == CAT_ERR_TEXT, "empty name");
    CHECK(parse_with(1, "Bad\x1b[2Jname", &e) == CAT_ERR_TEXT, "control code in name");
    CHECK(parse_with(14, "\xC9\xCD box drawing", &e) == CAT_ERR_TEXT, "top-half bytes in notes");

    CHECK(parse_with(2, "1969", &e) == CAT_ERR_NUMBER, "year before 1970");
    CHECK(parse_with(2, "19x0", &e) == CAT_ERR_NUMBER, "junk in year");
    CHECK(parse_with(4, "386SX", &e) == CAT_ERR_NUMBER, "cpu with letters");
    CHECK(parse_with(4, "686", &e) == CAT_ERR_NUMBER, "cpu not in the list");
    CHECK(parse_with(5, "0", &e) == CAT_ERR_NUMBER, "zero-byte download");
    CHECK(parse_with(5, "-5", &e) == CAT_ERR_NUMBER, "negative size");
    CHECK(parse_with(5, "99999999999", &e) == CAT_ERR_NUMBER, "size that wraps 32 bits");
    CHECK(parse_with(5, "67108865", &e) == CAT_ERR_NUMBER, "size over 64 MB");
    CHECK(parse_with(6, "0", &e) == CAT_ERR_NUMBER, "zero disk need");

    CHECK(parse_with(7, "1A2B3C4", &e) == CAT_ERR_CRC, "7-digit crc");
    CHECK(parse_with(7, "1A2B3C4DE", &e) == CAT_ERR_CRC, "9-digit crc");
    CHECK(parse_with(7, "1A2B3C4G", &e) == CAT_ERR_CRC, "non-hex crc");

    CHECK(parse_with(8, "SNAKE", &e) == CAT_ERR_EXE, "exe without extension");
    CHECK(parse_with(8, "SNAKE.TXT", &e) == CAT_ERR_EXE, "not a program");
    CHECK(parse_with(8, "..\\COMMAND.COM", &e) == CAT_ERR_EXE, "path in exe");
    CHECK(parse_with(8, "LONGNAME1.EXE", &e) == CAT_ERR_EXE, "9-character exe");
    CHECK(parse_with(8, "NUL.COM", &e) == CAT_ERR_EXE, "device name as exe");

    CHECK(parse_with(9, "TURBO", &e) == CAT_ERR_ENUM, "unknown profile");
    CHECK(parse_with(9, "clean", &e) == CAT_ERR_ENUM, "profile must be upper case");
    CHECK(parse_with(10, "GUS", &e) == CAT_ERR_ENUM, "unknown sound");
    CHECK(parse_with(11, "maybe", &e) == CAT_ERR_ENUM, "mouse neither yes nor no");

    CHECK(parse_with(13, "https://dos.castaliaos.com/A.ZIP", &e) == CAT_ERR_URL, "https is refused");
    CHECK(parse_with(13, "ftp://x/A.ZIP", &e) == CAT_ERR_URL, "ftp is refused");
    CHECK(parse_with(13, "http:///A.ZIP", &e) == CAT_ERR_URL, "no host");
    CHECK(parse_with(13, "http://host", &e) == CAT_ERR_URL, "no path");
    CHECK(parse_with(13, "http://ho st/A.ZIP", &e) == CAT_ERR_URL, "space in host");
    CHECK(parse_with(13, "http://host/A B.ZIP", &e) == CAT_ERR_URL, "space in path");
    CHECK(parse_with(13, "http://host/\"A.ZIP", &e) == CAT_ERR_URL, "quote in path");
    CHECK(parse_with(13, "http://host:123456/A.ZIP", &e) == CAT_ERR_URL, "six-digit port");
    CHECK(parse_with(13, "http://host:/A.ZIP", &e) == CAT_ERR_URL, "empty port");

    /* A refused line leaves nothing behind for a careless caller. */
    parse_with(13, "https://x/A.ZIP", &e);
    CHECK(e.id[0] == '\0' && e.size == 0, "a refused line leaves the entry zeroed");
}

int main(void)
{
    printf("CASTALIA catalogue index + CRC-32 unit tests\n");
    test_crc32();
    test_header();
    test_good();
    test_skip();
    test_refused();
    printf("\n%d checks, %d failure(s)\n", checks, failures);
    if (failures == 0)
        printf("ALL CATALOGUE INDEX TESTS PASSED\n");
    return failures ? 1 : 0;
}
