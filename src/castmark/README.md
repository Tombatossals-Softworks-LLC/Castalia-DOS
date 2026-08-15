# src/castmark — CASTMARK.EXE (prototype)

The Castalia system inspector + benchmark (`wmake castmark`). One screen:
hardware inspector (CPU class, **FNINIT/FNSTSW coprocessor probe** via the
shared `CPUDET` module, memory, XMS/EMS, VGA, floppies, ports, DOS, profile),
five fixed-duration benchmarks (CPU integer, FPU, memory copy, text video,
disk read) with animated gradient bars, deltas versus the previous saved run
(`C:\CASTALIA\CFG\CASTMARK.SCR`), and a provisional **Castalia Index**
(386SX/16 = 100).

Honesty notes: timing is BIOS ticks (18.2 Hz); the FPU mark is skipped when
no coprocessor is present; the disk mark tests the **current drive**; the
`bench_base[]` constants are provisional calibration anchors — tune them on
real hardware and log the runs in [`../../docs/APPS.md`](../../docs/APPS.md).

Links `ui`, `ini`, and `cpudet` (whose probes are byte-encoded machine
code in `#pragma aux`, verified by disassembly; behaviour on real silicon
is confirmed on hardware, not by the gcc gate).
