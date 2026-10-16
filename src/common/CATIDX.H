/* ===================================================================
 * CATIDX.H  -  The online game catalogue's index format
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * CATALOG.TXT is one header line and then one line per game, fields
 * separated by '|':
 *
 *   CASTALIA-CATALOG 1
 *   ID|NAME|YEAR|GENRE|CPU|SIZE|DISKKB|CRC32|EXE|PROFILE|SOUND|MOUSE|LICENSE|URL|NOTES
 *
 * Not INI, on purpose: the INI module holds one 16 KB file in memory,
 * and a catalogue is meant to outgrow that.  A line at a time can be
 * streamed through a fixed buffer however long the file gets.
 *
 * Everything in the file arrived over the network, so every field is
 * checked here before any tool acts on it: lengths, character sets,
 * 8.3 names, the closed lists of profiles and sounds, and that the
 * download is plain http:// (a 386 cannot speak TLS, and HTGET would
 * only report a redirect to https://).  A line that fails any check is
 * rejected whole; nothing half-parsed reaches the caller.
 *
 * The writer of these files is tools/catalog/mkcatalog.py, which
 * applies the same rules on the host.  C89, no allocation.
 * =================================================================== */

#ifndef CASTALIA_CATIDX_H
#define CASTALIA_CATIDX_H

#define CAT_MAGIC        "CASTALIA-CATALOG"
#define CAT_VERSION      1
#define CAT_FIELDS       15
#define CAT_LINE_MAX     512     /* longest line a reader must accept */

#define CAT_ID_MAX        8      /* also the folder under C:\GAMES      */
#define CAT_NAME_MAX     40
#define CAT_GENRE_MAX    14
#define CAT_EXE_MAX      12      /* 8.3                                 */
#define CAT_PROF_MAX      5
#define CAT_SOUND_MAX     5
#define CAT_LIC_MAX      24
#define CAT_URL_MAX     127
#define CAT_NOTES_MAX    76

/* A download larger than this is refused: no game for these machines
 * comes close, and it caps what a bad index can make a 40 MB disk do. */
#define CAT_SIZE_MAX     (64UL * 1024UL * 1024UL)

typedef struct {
    char          id[CAT_ID_MAX + 1];
    char          name[CAT_NAME_MAX + 1];
    int           year;                 /* 0 = not stated              */
    char          genre[CAT_GENRE_MAX + 1];
    int           cpu;                  /* 86, 286, 386, 486 or 586    */
    unsigned long size;                 /* bytes of the ZIP            */
    unsigned long disk_kb;              /* unpacked, in KB             */
    unsigned long crc;                  /* CRC-32 of the ZIP           */
    char          exe[CAT_EXE_MAX + 1];
    char          profile[CAT_PROF_MAX + 1];
    char          sound[CAT_SOUND_MAX + 1];
    int           mouse;                /* 1 = uses a mouse            */
    char          license[CAT_LIC_MAX + 1];
    char          url[CAT_URL_MAX + 1];
    char          notes[CAT_NOTES_MAX + 1];
} CAT_ENTRY;

/* Result codes. */
#define CAT_OK            0
#define CAT_SKIP          1     /* blank line or ';' comment           */
#define CAT_ERR_FIELDS  (-1)    /* not exactly CAT_FIELDS fields       */
#define CAT_ERR_ID      (-2)
#define CAT_ERR_TEXT    (-3)    /* name/genre/license/notes            */
#define CAT_ERR_NUMBER  (-4)    /* year/cpu/size/disk                  */
#define CAT_ERR_CRC     (-5)
#define CAT_ERR_EXE     (-6)
#define CAT_ERR_ENUM    (-7)    /* profile/sound/mouse                 */
#define CAT_ERR_URL     (-8)
#define CAT_ERR_HEADER  (-9)

/* 1 if 'line' is a header this reader understands ("CASTALIA-CATALOG 1",
 * trailing blanks/CR allowed).  A newer major version is refused rather
 * than misread. */
int cat_header_ok(const char *line);

/* Parse one line (modified in place) into 'e'.  Returns CAT_OK,
 * CAT_SKIP, or a CAT_ERR_* code; on anything but CAT_OK, 'e' is left
 * zeroed. */
int cat_parse_line(char *line, CAT_ENTRY *e);

/* Short English text for a result code. */
const char *cat_error_text(int code);

#endif /* CASTALIA_CATIDX_H */
