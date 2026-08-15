/* ===================================================================
 * INI.H  -  Minimal INI-file reader for CASTALIA DOS tools
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * A tiny, allocation-free INI parser sized for real-mode DOS and old
 * compilers (Open Watcom C, Turbo C 2.01).  It loads one file into a
 * single static buffer, splits it into lines in place, and answers
 * section / key queries by scanning that buffer.  No malloc, no heap
 * growth, no recursion.  C89 only.
 *
 * Grammar understood:
 *     ; comment            (also '#')
 *     [section]
 *     key = value          (spaces around '=' are trimmed)
 *
 * Section and key names are matched case-insensitively.
 * =================================================================== */

#ifndef CASTALIA_INI_H
#define CASTALIA_INI_H

/* Compile-time limits.  Tune for the target file; keep them small so the
 * static footprint stays modest on a 386SX. */
#define INI_MAX_SIZE      16384   /* max bytes of the loaded file        */
#define INI_MAX_LINES      1024   /* max non-empty logical lines         */
#define INI_MAX_SECTIONS     96   /* max [sections] tracked              */

/* Return codes for ini_open(). */
#define INI_OK            0
#define INI_ERR_OPEN      1       /* could not open the file             */
#define INI_ERR_READ      2       /* read error                          */
#define INI_ERR_TOOBIG    3       /* file exceeds INI_MAX_SIZE           */

/* Load a file into the module's static buffer, replacing any previous
 * contents.  Returns one of the INI_* codes above. */
int ini_open(const char *path);

/* Number of [sections] found in the currently-loaded file. */
int ini_section_count(void);

/* Name of section number 'index' (0-based), or NULL if out of range.
 * The pointer is valid until the next ini_open(). */
const char *ini_section_name(int index);

/* Value for key 'key' inside 'section', or NULL if not present.  The
 * returned pointer is into the static buffer and is trimmed and
 * NUL-terminated.  Valid until the next ini_open(). */
const char *ini_get(const char *section, const char *key);

/* Convenience: value or 'def' when the key is missing/empty. */
const char *ini_get_def(const char *section, const char *key,
                        const char *def);

/* Convenience: integer value (base 10) or 'def' when missing/invalid. */
int ini_get_int(const char *section, const char *key, int def);

/* Convenience: 1 if the value looks true (yes/on/true/1), 0 if it looks
 * false (no/off/false/0), else 'def'. */
int ini_get_bool(const char *section, const char *key, int def);

#endif /* CASTALIA_INI_H */
