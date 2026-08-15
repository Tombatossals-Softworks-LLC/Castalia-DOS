/* ===================================================================
 * DIRW.H  -  Portable DOS directory-scan wrapper for CASTALIA tools
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Turbo C uses findfirst/ffblk; Open Watcom uses _dos_findfirst/find_t.
 * This module hides the difference behind one tiny API so every tool
 * (CASTFM, CASTCOPY, ...) lists directories the same way.
 *
 * NOT re-entrant: one scan at a time (a single static state), which is
 * exactly how the DOS DTA works anyway.  C89.
 * =================================================================== */

#ifndef CASTALIA_DIRW_H
#define CASTALIA_DIRW_H

int  dirw_first(const char *mask);   /* 0 on success                    */
int  dirw_next(void);                /* 0 on success                    */
const char   *dirw_name(void);       /* name of the current entry       */
unsigned long dirw_size(void);       /* size in bytes                   */
int  dirw_isdir(void);               /* 1 if the entry is a directory   */

int  dirw_mkdir(const char *path);   /* 0 on success                    */

#endif /* CASTALIA_DIRW_H */
