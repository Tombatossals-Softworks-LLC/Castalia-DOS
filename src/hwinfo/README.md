# src/hwinfo — HWINFO.EXE (prototype)

> **Status:** implemented in [`HWINFO.C`](HWINFO.C); builds with
> `wmake hwinfo`. It performs the safe, no-assembly detections below, plus
> serial/parallel port counts from the BIOS equipment word.
>
> **CPU class:** under Open Watcom, two small `#pragma aux` inline-assembly
> probes refine the CPU report — whether CPUID exists (togglable EFLAGS ID
> bit) and, if so, the CPUID family — distinguishing 386/486-class (no
> CPUID) from Pentium-class and later. CPUID is executed only when the
> ID-bit test says it is safe, so it runs on a plain 386. On any other
> compiler (including the gcc CI syntax-check) the code falls back to the
> guaranteed 386 baseline string. That inline-assembly path is the one
> piece **validated on a real Open Watcom build**, not by the gcc gate.

Hardware diagnostics. Reports CPU class, FPU presence, conventional/extended
memory, EMS/XMS availability, VGA presence, mouse-driver presence, CD-ROM
(MSCDEX) presence, the sound environment, the reported DOS version, free disk
space, and the current boot profile.

Detection is done with BIOS interrupts, DOS calls, and small instruction
probes, and is honest about what DOS cannot reliably detect (exact CPU MHz,
exact sound-card model, unprobed IRQ/DMA, CF-vs-HDD). The full detection method,
pseudocode, and a portable C + inline-assembly strategy (bridging the Turbo C
`.x` vs Open Watcom `.w` `REGS` difference) are in
[`../../docs/DIAGNOSTICS.md`](../../docs/DIAGNOSTICS.md).

Shares `../common/ui.*` (and `../common/ini.*` for reading the profile).
