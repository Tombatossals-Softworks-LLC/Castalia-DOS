# Real-hardware run: 386SX, 2026-10-01

```
CASTALIA DOS test run
Build .......: 1.0 "Tombatossals" as shown on screen (image commit not recorded)
Date ........: 2026-10-01
Tester ......: dabellan
Environment .: Real 386-class PC, reported by the tester as a 386SX;
               387 fitted, 639 KB base + ~3.2 MB XMS free, VGA on a CRT
Media .......: Installed system on C: (436,176 KB free)
```

This is the first run of CASTALIA DOS on real hardware rather than an
emulator. The installed system booted on two profiles, the menu, the game
launcher, `CASTMARK`, `HWINFO` and `CASTID` all ran, and a VGA game started
from the launcher. One thing failed: the `CASTMARK` disk benchmark stopped
with a DOS critical error writing to `C:`.

Only what the photos show is recorded below. Everything else in the
`TESTING.md` matrix is still `(pending)` for this machine.

## Evidence

Eleven photos of the CRT, taken over about eleven minutes. The machine's own
clock appears on four of them and runs a steady 4 min 11 s ahead of the
camera's EXIF time, which is consistent with a single live session across two
boots. Times below are the machine's clock.

| Photo | Machine time | Profile | What it shows |
|---|---|---|---|
| IMG_5565 | 12:40:24 | CLEAN | Main menu, `386SX Edition - 1.0 "Tombatossals"`, status bar `Profile: CLEAN` |
| IMG_5566, IMG_5567 | ~12:41 | CLEAN | *The Secret of Monkey Island* (Spanish VGA release) started from Launch Games, in game |
| IMG_5568 | ~12:42 | CLEAN | `CASTMARK`: inspector plus four benchmark results; run sitting at `Running 5/5: Disk read` |
| IMG_5569 | ~12:44 | EMS | `CASTMARK` after a reboot into EMS: same four results; again at `Running 5/5: Disk read` |
| IMG_5570 | ~12:50 | EMS | `Error writing to drive C: DOS area: drive not ready` / `(A)bort, (I)gnore, (R)etry, (F)ail?` over the `CASTMARK` screen |
| IMG_5571, IMG_5575 | ~12:50, ~12:51 | EMS | `HWINFO` system report |
| IMG_5572 to IMG_5574 | 12:50:51 to 12:51:02 | EMS | `CASTID` signature card, uptime advancing |

The photos are kept outside the repository.

What the photos do not settle: the machine itself is out of frame, and no
software test can tell an SX from a DX (`HWINFO` says so on screen). The 386SX
identification rests on the tester; the software confirms an 80386 with a 387.

## What the machine reported

`HWINFO` on the EMS profile:

| Row | Value |
|---|---|
| Processor | 80386 (SX/DX look alike to software) |
| Math coprocessor | present (FNSTSW probe) |
| Ports | 0 serial (COM), 0 parallel (LPT) |
| Conventional memory | 639 KB total |
| Extended memory | 2926 KB free (XMS driver 3.00) |
| EMS | available |
| Video adapter | VGA or better |
| Mouse | driver present (2 buttons) |
| CD-ROM | none (EMS profile, as expected) |
| Sound (BLASTER) | A220 I5 D1 T4 |
| Kernel | CASTALIA build 1 (OEM CAh, ed 1) |
| DOS version | reports 7.10 |
| Disk space | 436176 KB free on current drive |

On CLEAN, `CASTMARK` showed 3206 KB XMS free and no EMS. The 280 KB less on
EMS is consistent with what `JEMM386` takes on that profile.

The `BLASTER` value is the `AUTOEXEC.BAT` default used when `SOUND.BAT` is
missing, so it does not show that a sound card was found. The BIOS reports no
COM ports, so the mouse `CTMOUSE` found is presumably PS/2.

## Results

| Test ID | Profile | Actual result | Pass/Fail | Notes |
|---|---|---|---|---|
| BOOT-04 | CLEAN | Booted to the Castalia menu; status bar `Profile: CLEAN`; `CASTMARK` shows XMS yes, EMS no | Pass | Driver list during boot not photographed |
| BOOT-06 | EMS | Booted; XMS 3.00 and EMS both present; `CASTID` and `HWINFO` report profile EMS | Pass | Frame address and UMB use not captured. Under QEMU `Jemm386` found no page frame; this machine has one |
| HW-02 | CLEAN, EMS | Installed system on `C:` boots to the menu on real hardware | Partial | Run used CLEAN and EMS, not the default XMS profile. Disk type (IDE or CF) not recorded |
| GAME (no ID) | CLEAN | *Monkey Island* started from Launch Games and ran in VGA | Pass | Sound not checked |
| Kernel API | EMS | `CASTID`: build 1, edition 01h, OEM CAh, profile EMS, live uptime | Pass | First check of the kernel identity calls on real hardware |
| HWINFO | EMS | Full report, values as above | Pass | Run from the menu, not via the DIAG profile |
| CASTMARK | CLEAN, EMS | CPU, FPU, memory and video benchmarks complete | Pass | Raw numbers below |
| CASTMARK disk | CLEAN | Stopped at `Running 5/5: Disk read`; the next photo is after a reboot | Fail | See below |
| CASTMARK disk | EMS | DOS critical error writing to `C:`; no disk result | Fail | See below |

Not run on this machine: `MEM /C` targets (MEM-01 to MEM-08), the XMS, CDROM,
WIN3X, DIAG, SAFE and PROMPT profiles, floppy boot (BOOT-01, HW-01), install
(INST-nn), sound, PC speaker, CD-ROM and CD audio, `CASTLINK`, `UNDEL`.

## CASTMARK against the anchors

The anchors in `src/castmark/CASTMARK.C` come from an 86Box machine set up as
the reference 386SX/16. This machine gave identical numbers on CLEAN and EMS:

| Benchmark | This machine | 86Box anchor | Ratio |
|---|---|---|---|
| CPU integer | 44 kOps/s | 38 | x1.2 |
| FPU (80x87) | 117 kFLOP/s | 136 | x0.9 |
| Memory copy | 2683 KB/s | 6656 | x0.4 |
| Video (text) | 22.1 scr/s | 20.1 | x1.1 |
| Disk read | no result | 530 | - |

CPU, FPU and video are close to the emulator. Memory copy is 2.5 times slower
than the emulator, so the `CASTMARK.C` comment that this anchor "should hold on
metal" does not hold on this machine. It could be RAM wait states on this board
or an optimistic 86Box memory model. One machine is not enough to tell which,
so the anchors are left as they are.

## The disk benchmark failure

The disk benchmark is called "Disk read", but it first writes a 48 KB file,
`CMARK$$.TMP`, in the current directory. `AUTOEXEC.BAT` never changes
directory, so that is `C:\`. Creating a file in the root directory writes the
FAT and the root directory, which DOS reports as the "DOS area". The write
failed with "drive not ready". That status comes from the BIOS disk call, so
the fault is below `CASTMARK`: the drive, the controller, the BIOS, or how the
kernel drives this BIOS. No profile in the run loads a disk cache or disk
driver.

What is still unknown, and what to do next on the machine:

1. Run `CHKDSK C:` without `/F` first. A write that failed partway through the
   FAT or root directory can leave lost clusters. Delete `C:\CMARK$$.TMP` if
   it is there.
2. Check whether any write to `C:` works. At the prompt, on CLEAN and on SAFE:
   `ECHO test > C:\T1.TXT`, then `COPY C:\AUTOEXEC.BAT C:\TEMP\`. If these
   fail the same way, this is a disk write problem, not a benchmark problem.
   It then also affects `SETUP`, `SETSOUND`, `CFGEDIT` and `CASTMARK`'s own
   score save.
3. Record what `C:` is (IDE drive model, or CF card and adapter), the BIOS
   drive type and geometry, and how long the screen stayed at
   `Running 5/5` before the error. The photos suggest a long wait: on EMS the
   error is in a photo about six minutes after the earlier `Running 5/5`
   photo, if both are the same run.

Separately from the cause, the build tested had no critical-error handler in
any tool, so the disk error showed up as the raw DOS prompt drawn over the UI.
Every tool now installs one through `ui_init()`: the benchmark reports
"failed: drive not ready" and the index is computed from the other four. It
also writes its test file to `%TEMP%` rather than the root of `C:`.
