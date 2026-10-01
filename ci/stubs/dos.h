/* ci/stubs/dos.h  -  HOST SYNTAX-CHECK STUB ONLY. NOT A REAL DOS HEADER.
 *
 * This minimal <dos.h> lets a modern host compiler (gcc/clang) syntax-check
 * the CASTALIA DOS sources, which #include <dos.h> and use union REGS /
 * int86 / MK_FP.  It is used ONLY by scripts/check.sh via -Ici/stubs and is
 * NEVER compiled into a real build - the real tools are built with Open
 * Watcom or Turbo C, which ship the genuine <dos.h>.
 *
 * The field layout mirrors the .x (16-bit word), .w (Watcom word), and
 * .h (byte) accessors the code uses, so the syntax check is meaningful.
 */
#ifndef CASTALIA_STUB_DOS_H
#define CASTALIA_STUB_DOS_H

#include <stddef.h>

struct WORDREGS {
    unsigned short ax, bx, cx, dx, si, di, cflag, flags;
};
struct BYTEREGS {
    unsigned char al, ah, bl, bh, cl, ch, dl, dh;
};
union REGS {
    struct WORDREGS x;
    struct WORDREGS w;
    struct BYTEREGS h;
};
struct SREGS { unsigned short es, cs, ss, ds; };

int int86(int intno, union REGS *inregs, union REGS *outregs);
int int86x(int intno, union REGS *inregs, union REGS *outregs,
           struct SREGS *segregs);
void segread(struct SREGS *sregs);

/* size_t, not unsigned long: a pointer fits size_t on both LP64 (Linux)
 * and LLP64 (64-bit Windows, where long is only 32 bits). */
#ifndef MK_FP
#define MK_FP(seg, ofs) ((void *)(((size_t)(seg) << 16) | (unsigned)(ofs)))
#endif
#ifndef FP_SEG
#define FP_SEG(p) ((unsigned short)(((size_t)(void *)(p)) >> 16))
#define FP_OFF(p) ((unsigned short)((size_t)(void *)(p)))
#endif

/* --- Open-Watcom-style directory search (host syntax-check only) ----
 * The real declarations come from Open Watcom's <dos.h>; Turbo C uses a
 * different API guarded by __TURBOC__ in the sources.  These let the gcc
 * gate compile the Watcom code path.
 */
#define _A_NORMAL 0x00
#define _A_RDONLY 0x01
#define _A_HIDDEN 0x02
#define _A_SYSTEM 0x04
#define _A_VOLID  0x08
#define _A_SUBDIR 0x10
#define _A_ARCH   0x20

struct find_t {
    char           reserved[21];
    char           attrib;
    unsigned short wr_time;
    unsigned short wr_date;
    unsigned long  size;
    char           name[13];
};
int _dos_findfirst(const char *path, unsigned attributes, struct find_t *buf);
int _dos_findnext(struct find_t *buf);

#endif /* CASTALIA_STUB_DOS_H */
