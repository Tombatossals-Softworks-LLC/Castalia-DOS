/* ===================================================================
 * DIRW.C  -  Portable DOS directory-scan wrapper for CASTALIA tools
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * See DIRW.H.  C89, Turbo C + Open Watcom (+ host syntax-check stubs).
 * =================================================================== */

#include <dos.h>
#if defined(__WATCOMC__)
#include <direct.h>
#elif defined(__TURBOC__)
#include <dir.h>
#else
#include <sys/stat.h>       /* host syntax-check only */
#endif
#include "DIRW.H"

#if defined(__TURBOC__)

static struct ffblk g_ff;

int dirw_first(const char *mask)
{
    return findfirst(mask, &g_ff,
                     FA_DIREC | FA_RDONLY | FA_HIDDEN | FA_SYSTEM);
}
int dirw_next(void)            { return findnext(&g_ff); }
const char *dirw_name(void)    { return g_ff.ff_name; }
unsigned long dirw_size(void)  { return (unsigned long)g_ff.ff_fsize; }
int dirw_isdir(void)           { return (g_ff.ff_attrib & FA_DIREC) != 0; }

#else

static struct find_t g_ft;

int dirw_first(const char *mask)
{
    return _dos_findfirst(mask,
                          _A_SUBDIR | _A_RDONLY | _A_HIDDEN | _A_SYSTEM,
                          &g_ft);
}
int dirw_next(void)            { return _dos_findnext(&g_ft); }
const char *dirw_name(void)    { return g_ft.name; }
unsigned long dirw_size(void)  { return g_ft.size; }
int dirw_isdir(void)           { return (g_ft.attrib & _A_SUBDIR) != 0; }

#endif

int dirw_mkdir(const char *path)
{
#if defined(__WATCOMC__) || defined(__TURBOC__)
    return mkdir(path);
#else
    return mkdir(path, 0777);   /* host syntax-check only */
#endif
}
