/* ===================================================================
 * CPUDET.C  -  CPU / FPU detection shared by CASTALIA tools
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * BUILD NOTE: the probes below are emitted as raw machine code inside
 * the #pragma aux clauses.  The 16-bit Open Watcom compiler refuses
 * 32-bit register names in pragma register lists and refuses CPUID /
 * FNSTSW mnemonics regardless of the -N CPU switch (confirmed on the
 * CI toolchain), but byte-encoded pragmas bypass the inline assembler
 * entirely, so this file compiles with the ordinary -0 flags.  Every
 * byte sequence was verified by disassembly (objdump, 16-bit mode);
 * the intended instructions are shown beside each byte.
 *
 * Runtime safety: 386+ instructions (PUSHFD et al.) are fine - a 386
 * is the product's minimum CPU - and CPUID is executed only after the
 * EFLAGS ID-bit toggle proves it exists.  The gcc CI gate compiles the
 * portable fallbacks in the #else branches.
 * =================================================================== */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include "CPUDET.H"

#ifdef __WATCOMC__

/* Can the EFLAGS ID bit (21) be toggled?  Yes -> CPUID exists.
 * (Upper halves of EAX/ECX are clobbered; harmless to 16-bit code.) */
extern int cpu_has_cpuid(void);
#pragma aux cpu_has_cpuid =         \
    0x66 0x9C                       /* pushfd            */ \
    0x66 0x58                       /* pop  eax          */ \
    0x66 0x89 0xC1                  /* mov  ecx, eax     */ \
    0x66 0x35 0x00 0x00 0x20 0x00   /* xor  eax, 200000h */ \
    0x66 0x50                       /* push eax          */ \
    0x66 0x9D                       /* popfd             */ \
    0x66 0x9C                       /* pushfd            */ \
    0x66 0x58                       /* pop  eax          */ \
    0x66 0x51                       /* push ecx          */ \
    0x66 0x9D                       /* popfd             */ \
    0x66 0x31 0xC8                  /* xor  eax, ecx     */ \
    0x66 0xC1 0xE8 0x15             /* shr  eax, 21      */ \
    0x25 0x01 0x00                  /* and  ax, 1        */ \
    value [ax] modify [ax cx];

/* CPUID leaf 1 -> family nibble. */
extern int cpu_id_family(void);
#pragma aux cpu_id_family =         \
    0x66 0xB8 0x01 0x00 0x00 0x00   /* mov  eax, 1       */ \
    0x0F 0xA2                       /* cpuid             */ \
    0x66 0xC1 0xE8 0x08             /* shr  eax, 8       */ \
    0x25 0x0F 0x00                  /* and  ax, 0Fh      */ \
    value [ax] modify [ax bx cx dx];

/* Can the EFLAGS AC bit (18) be toggled?  386 = no, 486+ = yes. */
extern int cpu_has_ac(void);
#pragma aux cpu_has_ac =            \
    0x66 0x9C                       /* pushfd            */ \
    0x66 0x58                       /* pop  eax          */ \
    0x66 0x89 0xC1                  /* mov  ecx, eax     */ \
    0x66 0x35 0x00 0x00 0x04 0x00   /* xor  eax, 40000h  */ \
    0x66 0x50                       /* push eax          */ \
    0x66 0x9D                       /* popfd             */ \
    0x66 0x9C                       /* pushfd            */ \
    0x66 0x58                       /* pop  eax          */ \
    0x66 0x51                       /* push ecx          */ \
    0x66 0x9D                       /* popfd             */ \
    0x66 0x31 0xC8                  /* xor  eax, ecx     */ \
    0x66 0xC1 0xE8 0x12             /* shr  eax, 18      */ \
    0x25 0x01 0x00                  /* and  ax, 1        */ \
    value [ax] modify [ax cx];

/* Classic FPU probe: preload AX, FNINIT, FNSTSW AX.  With an FPU the
 * status low byte reads back 0; without one AX keeps the preload.
 * No-wait forms are used on purpose so a missing FPU cannot hang. */
extern unsigned fpu_status(void);
#pragma aux fpu_status =            \
    0xB8 0x5A 0x5A                  /* mov  ax, 5A5Ah    */ \
    0xDB 0xE3                       /* fninit            */ \
    0xDF 0xE0                       /* fnstsw ax         */ \
    value [ax] modify [ax];

#else

/* BIOS equipment word (INT 11h) - fallback probes only. */
static unsigned equip(void)
{
    union REGS r;
    int86(0x11, &r, &r);
    return r.x.ax;
}

#endif /* __WATCOMC__ */

void cpu_describe(char *out)
{
#ifdef __WATCOMC__
    if (cpu_has_cpuid()) {
        int fam = cpu_id_family();
        if (fam >= 6)
            sprintf(out, "80686-class or later (CPUID family %d)", fam);
        else if (fam == 5)
            strcpy(out, "Pentium-class (CPUID family 5)");
        else
            sprintf(out, "80486-class (CPUID family %d)", fam);
    } else if (cpu_has_ac()) {
        strcpy(out, "80486-class (no CPUID)");
    } else {
        strcpy(out, "80386 (SX/DX look alike to software)");
    }
#else
    strcpy(out, "80386-class or later (minimum required)");
#endif
}

void cpu_describe_short(char *out)
{
#ifdef __WATCOMC__
    if (cpu_has_cpuid()) {
        int fam = cpu_id_family();
        if (fam >= 6)
            sprintf(out, "80686+ (family %d)", fam);
        else if (fam == 5)
            strcpy(out, "Pentium-class");
        else
            strcpy(out, "80486-class");
    } else if (cpu_has_ac()) {
        strcpy(out, "80486-class");
    } else {
        strcpy(out, "80386 (SX or DX)");
    }
#else
    strcpy(out, "80386-class or later");
#endif
}

int fpu_probe_kind(void)
{
#ifdef __WATCOMC__
    return 1;
#else
    return 0;
#endif
}

int fpu_present(void)
{
#ifdef __WATCOMC__
    return ((fpu_status() & 0xFFu) == 0);
#else
    return (equip() & 0x0002) ? 1 : 0;
#endif
}
