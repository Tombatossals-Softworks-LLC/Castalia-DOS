/* ===================================================================
 * SAFEIO.H  -  Crash-safe file writing for CASTALIA DOS tools
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Opening a file with fopen(name, "wb") empties it at once.  If the
 * write that follows fails - disk full, a bad sector, a floppy pulled
 * out - the old contents are already gone and only a stub is left.
 * The tools that write CONFIG.SYS, GAMES.INI, SOUND.BAT and copies of
 * user files avoid that by writing a complete new file under a
 * temporary name in the same directory and only then swapping it in.
 *
 * The temporary names are fixed 8.3 names (CASTNEW.$$$, CASTCPY.$$$,
 * CASTOLD.$$$): DOS runs one program at a time, so they cannot clash
 * with another writer, and a leftover after a crash is recognisable.
 *
 * No dynamic allocation.  C89, Open Watcom / Turbo C.
 * =================================================================== */

#ifndef CASTALIA_SAFEIO_H
#define CASTALIA_SAFEIO_H

#include <stdio.h>

/* Path buffers handed to sio_tmpname() must hold this many bytes: an
 * 80-character path plus a 12-character file name, with room spare. */
#define SIO_PATH        144

/* Return codes. */
#define SIO_OK           0
#define SIO_ERR_SRC     (-1)    /* source missing or unreadable          */
#define SIO_ERR_DST     (-2)    /* new copy could not be written in full */
#define SIO_ERR_SAME    (-3)    /* source and destination are one file   */
#define SIO_ERR_NAME    (-4)    /* path too long for SIO_PATH            */
#define SIO_ERR_SWAP    (-5)    /* new copy could not be put in place    */

/* 1 if 'path' names a file that can be opened for reading. */
int sio_exists(const char *path);

/* 1 if both names reach the same file.  DOS accepts the same file as
 * GAMES.INI, games.ini, .\GAMES.INI or C:GAMES.INI, so the names are
 * canonicalised (INT 21h AH=60h) before they are compared. */
int sio_same(const char *a, const char *b);

/* The temporary name to write a new version of 'final' under: the same
 * directory, file name CASTNEW.$$$.  'out' holds SIO_PATH bytes.
 * Returns SIO_OK or SIO_ERR_NAME. */
int sio_tmpname(const char *final, char *out);

/* fclose() that also reports earlier write errors on the stream.  The
 * last buffered write only reaches the disk inside fclose(), so a full
 * disk often shows up nowhere else.  0 if every write landed. */
int sio_close(FILE *fp);

/* Put the complete file 'tmp' in place of 'final'.  The old 'final' is
 * moved aside first and only deleted once 'tmp' carries its name, so a
 * failure at any step leaves the old file where it was.  'tmp' is
 * removed on failure.  Returns SIO_OK, SIO_ERR_SAME, SIO_ERR_NAME or
 * SIO_ERR_SWAP. */
int sio_replace(const char *tmp, const char *final);

/* Copy 'src' to 'dst' through a temporary file next to 'dst', so an
 * existing 'dst' survives any failure untouched and no partial copy is
 * ever left behind.  Refuses to copy a file onto itself. */
int sio_copy(const char *src, const char *dst);

#endif /* CASTALIA_SAFEIO_H */
