/* ===================================================================
 * CPUDET.H  -  CPU / FPU detection shared by CASTALIA tools
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * One place for the processor probes (HWINFO and CASTMARK both use it).
 * Under Open Watcom this module carries byte-encoded machine-code
 * probes (EFLAGS AC/ID bit toggles, CPUID, FNINIT/FNSTSW) in #pragma
 * aux clauses; on any other compiler - including the gcc CI syntax
 * gate - portable fallbacks are compiled instead.
 * =================================================================== */

#ifndef CASTALIA_CPUDET_H
#define CASTALIA_CPUDET_H

/* Fill 'out' (>= 48 bytes) with a human CPU description:
 *   386-class / 486-class / Pentium-class / 686+ (CPUID family).
 * A 386SX and 386DX cannot be told apart in software; we say so. */
void cpu_describe(char *out);

/* Same identification in at most 24 characters, for panels too narrow
 * for the long form.  The 386 line keeps the "SX or DX" caveat: it is
 * the whole point of the string, and dropping it would turn an honest
 * limitation into a false claim.  `out` needs 32 bytes. */
void cpu_describe_short(char *out);

/* 1 if a math coprocessor is present.  Under Watcom this is the real
 * FNINIT/FNSTSW instruction probe (authoritative for a 386+387 pair);
 * elsewhere it falls back to the BIOS equipment-word bit. */
int fpu_present(void);

/* 1 = instruction probe (FNSTSW), 0 = BIOS equipment bit fallback. */
int fpu_probe_kind(void);

#endif /* CASTALIA_CPUDET_H */
