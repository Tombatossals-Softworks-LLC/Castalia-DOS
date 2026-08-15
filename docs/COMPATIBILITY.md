# 3. Compatibility Goals

**CASTALIA DOS — Technical Bible, Section 3**
**Applies to:** CASTALIA DOS 386SX Edition (1.0 "Tombatossals" target)
**Status:** Normative for 1.0. Tier D items are explicitly out of scope for 1.0.

---

## 3.0 Purpose and Guiding Principle

Castalia DOS exists to run **real DOS software on real retro hardware** — with a
386SX as the reference floor machine. This section defines, honestly and in
detail, *what we promise to run*, *what we try hard to run*, and *what we do not
chase* for the 1.0 release.

The governing rule of the whole project applies here without exception:

> **Compatibility beats elegance.**

We would rather ship a boot menu with seven memory profiles that looks slightly
busy than ship one clever unified profile that breaks Ultima VII. When a design
choice trades a nicer UI against a game that stops working, the game wins.

Three things shape every compatibility decision:

1. **The 386SX is the floor, not the target.** If a title needs a 486 to be
   enjoyable, we say so plainly rather than pretending a profile can fix physics.
2. **We are a DOS-compatible environment, not an emulator.** We cannot slow a CPU
   that is genuinely too fast, nor speed one that is genuinely too slow. We can
   only manage memory, drivers, and configuration around the game.
3. **We ship only open/free components.** FreeDOS kernel, FreeCOM, HIMEMX,
   JEMM386, CTMOUSE, UIDE/SHSUCDX, KEYB. No Microsoft code, binaries, or manual
   text is ever bundled. This constrains *how* we achieve compatibility, never
   the honesty of *what* we claim.

Compatibility is expressed two ways in this document:

- **Tiers (A/B/C/D)** — the *promise level* for a class of software.
- **Per-game verdicts** — the *lived reality* on each reference CPU.

A title can sit in Tier A as a class ("real-mode DOS games must work") yet still
earn an honest "Unplayable on 386SX" verdict if it is a 1993+ texture-mapped
title. The tier says *it will run and is supported*; the verdict says *how well
it runs on which iron*. Keep the two separate in your head.

---

## 3.1 Compatibility Tiers

### Tier summary

| Tier | Label              | Promise                                                        | 1.0 scope |
|------|--------------------|---------------------------------------------------------------|-----------|
| A    | Must work          | Ships broken = release blocker. Tested every build.           | In scope  |
| B    | Should work        | Expected to work; a failure is a bug we fix before 1.0.       | In scope  |
| C    | Best effort        | We try; some titles need per-game tuning or won't fully work. | Partial   |
| D    | Not a 1.0 priority | Explicitly deferred or out of charter. Do not block on these. | Out       |

---

### Tier A — "Must work"

**Definition.** The core DOS experience. If any Tier A item regresses, the build
is not shippable. These run under the **CLEAN** profile (HIMEMX + `DOS=HIGH`,
~615 KB free conventional) unless the user chooses otherwise, and most also run
under **XMS**.

| Tier A capability                    | Notes                                                     |
|--------------------------------------|-----------------------------------------------------------|
| Real-mode DOS games (1988–1992 era)  | 8086/286/386 real-mode. The heart of the library.         |
| DOS utilities                        | Norton-style tools, file managers, editors, packers.      |
| Batch files (`.BAT`)                 | Full FreeCOM batch semantics; `AUTOEXEC.BAT` menus.       |
| FAT12 (floppy)                       | 360K/720K/1.2M/1.44M read/write, boot.                    |
| FAT16 (hard disk / CompactFlash)     | Up to 2 GB partitions; IDE and CF via UIDE.               |
| Keyboard (US + intl via `KEYB`)      | Scancode-accurate; ES/DE/FR/UK layouts included.          |
| VGA 80×25 text mode, 16 colors       | The Castalia UI itself; also thousands of text apps.      |
| VGA graphics (Mode 13h, Mode X, EGA) | 320×200×256 and planar modes — the bulk of DOS games.     |
| Basic mouse (serial + PS/2)          | CTMOUSE; INT 33h; two/three button.                       |
| PC Speaker + AdLib (OPL2) audio      | Baseline sound that needs no card configuration.          |

**Representative titles that must work flawlessly:** Prince of Persia, Commander
Keen, The Secret of Monkey Island, SimCity, Lemmings, Sid Meier's Civilization,
Dune II, King's Quest V, Wolfenstein 3D. On the reference 386SX these are
**Good to Excellent** across the board.

---

### Tier B — "Should work"

**Definition.** The extended DOS experience that depends on a memory manager, a
sound card, or a CD-ROM. Supported and expected to work; a failure is a bug we
fix before 1.0. These map to the **XMS**, **EMS**, **CDROM**, and **WIN3X**
profiles.

| Tier B capability                       | Profile        | Notes                                          |
|-----------------------------------------|----------------|------------------------------------------------|
| XMS-using games                         | XMS            | Extended memory via HIMEMX. Doom, Warcraft.    |
| EMS-using games                         | EMS            | JEMM386 page frame at E000. X-COM, UUW.        |
| CD-ROM games (data + CD audio)          | CDROM          | UIDE + SHSUCDX; ATAPI CD; Red Book audio.      |
| Sound Blaster family games              | any + SETSOUND | SB 1.5/2.0/Pro/16 via `SET BLASTER`.           |
| Windows 3.1 **standard mode**           | WIN3X          | 286-protected mode Windows on XMS.             |
| Common DOS installers                   | any            | `INSTALL.EXE`/`SETUP.EXE`, disk-swap prompts.  |

**Notes on Tier B scope.**

- *Windows 3.1 standard mode* (`WIN /S`) is Tier B; **386 enhanced mode is
  Tier C** (see below) and **WfW 3.11 enhanced mode is best-effort**.
- *Sound Blaster* means the documented SB command set through SB16. GUS
  (Gravis UltraSound) and wavetable daughterboards are best-effort (Tier C).
- *CD audio* (Red Book, played by the drive) and *CD data* (files via SHSUCDX)
  are different subsystems; a game can need one, the other, or both. See §3.5.

---

### Tier C — "Best effort"

**Definition.** Titles and subsystems that push against the limits of the
reference hardware, the open toolchain, or DOS itself. We attempt them, we
provide per-game GAMES.INI tuning, and we document the rough edges. Some will run
well only on a 486 or Pentium; some will need manual intervention.

| Tier C capability                         | Why it's best-effort                                     |
|-------------------------------------------|----------------------------------------------------------|
| DOS extenders (VCPI/DOS4GW-class)         | Protected-mode host interaction; fine on 486, hard on SX.|
| DPMI games                                | Need a DPMI provider; JEMM386 supplies VCPI, not DPMI.   |
| Late 486/Pentium titles (1995–1996)       | Duke3D, Descent, Magic Carpet — CPU-bound past the floor. |
| Unusual copy protection                   | Disk-timing checks, laser holes, dongles, CD checks.     |
| WfW 3.11 **enhanced mode**                | 386 enhanced + VxDs stress the EMM and timing model.     |
| Gravis UltraSound / wavetable             | Non-SB APIs; driver availability varies.                 |
| VESA VBE 2.0 hi-res game modes            | Needs a VBE TSR (UniVBE-class) on cards lacking VBE 2.0.  |
| Roland MT-32 / General MIDI (MPU-401)     | Works, but MT-32 needs the real module or careful setup. |

**How Tier C is handled.** Each affected title gets a `[Game]` section in
`GAMES.INI` with the flags from §3.6. `LAUNCH.EXE` reads those flags, selects the
right boot profile (or asks the user to reboot into it), sets `BLASTER`, and
loads VESA/CD helpers only when needed. Where hardware simply isn't fast enough,
we say so in the verdict table rather than hiding it behind a profile.

---

### Tier D — "Not a 1.0 priority"

**Definition.** Out of charter for 1.0, or a different product entirely. Listed so
nobody wastes a release cycle on them. Some are deferred to 1.1 "Montornés" or
later; some are permanently outside what a FreeDOS-based real-mode environment
should attempt.

| Tier D item                          | Verdict for 1.0 | Rationale                                        |
|--------------------------------------|-----------------|--------------------------------------------------|
| Full Windows 95 replacement          | Out of charter  | Castalia is DOS-compatible, not a Win9x clone.   |
| Protected-mode kernel                | Out of charter  | Kernel is real-mode FreeDOS `KERNEL.SYS`.        |
| Preemptive multitasking              | Out of charter  | DOS is single-tasking; TSRs only.                |
| Long filenames (VFAT/LFN)            | Deferred        | 8.3 only in 1.0. LFN muddies FAT compatibility.  |
| Networking by default                | Deferred        | Optional packet-driver stack possible post-1.0.  |
| USB (storage, HID, audio)            | Deferred        | No open real-mode USB stack we can rely on.      |
| Modern filesystems (FAT32/NTFS/ext)  | Out for 1.0     | FAT12/FAT16 only; FAT32 is a 1.1 investigation.  |
| GUI desktop / windowing shell        | Out of charter  | Text-mode UI is the product's identity.          |

**Note on FAT32 and LFN.** These are *deferred*, not *forbidden*: they may be
revisited in 1.1+. They are Tier D for 1.0 because each one weakens the "behaves
like MS-DOS 5.0/6.22, FAT12/FAT16" promise that everything else is tested
against. Networking, likewise, can be added as an *optional* packet-driver load
without becoming a default.

---

## 3.2 Verdict Legend

The per-game table uses a fixed vocabulary. Read it strictly — "Playable-slow" is
a real, distinct rating, not a synonym for "Good."

| Verdict         | Meaning                                                                     |
|-----------------|-----------------------------------------------------------------------------|
| **Excellent**   | Runs perfectly with headroom; the intended experience or better.            |
| **Good**        | Fully playable and smooth; no meaningful compromise.                        |
| **Playable**    | Enjoyable at intended settings; minor slowdowns in worst-case scenes.       |
| **Playable-slow** | Runs and is *finishable*, but below ideal framerate. Fine for turn-based/adventure; choppy for action. |
| **Marginal**    | Technically runs; low framerate, frequent stutter. Only for patient players. |
| **Unplayable**  | Too slow to enjoy, or fails to run acceptably on this CPU.                   |
| **Too fast**    | Runs, but timing loops make it play too fast; needs a `slowdown` measure.    |
| **N/A**         | Not applicable (e.g. profile/feature not relevant to that title).            |

**CPU floor** is the *practical* minimum for a title to run at all, which is often
above the box's optimistic printed minimum. **Recommended profile** is the
Castalia boot profile `LAUNCH.EXE` will select by default.

---

## 3.3 Per-Game Compatibility Table

Reference CPUs: **386SX** = 16/25 MHz, 16-bit external bus, usually no FPU.
**386DX** = 25/33 MHz, 32-bit bus, optional 387. **486** = 486DX/33 to DX2/66
(FPU on-die from DX). **Pentium** = P60–P133 class.

Sound column lists the best-supported devices, in preference order. "MT-32" =
Roland MT-32 via MPU-401; "GM" = General MIDI.

| Game | Year | CPU floor | Profile | Sound | 386SX | 386DX | 486 | Pentium | Notes |
|------|------|-----------|---------|-------|-------|-------|-----|---------|-------|
| Prince of Persia | 1990 | 8086 | CLEAN | PC Spkr, AdLib | Excellent | Excellent | Excellent | Excellent | Frame-timed; safe on fast CPUs. Real mode. |
| Commander Keen (4–6) | 1990 | 286 | CLEAN | AdLib, PC Spkr | Excellent | Excellent | Excellent | Excellent | Smooth EGA scroll wants 286+. `slowdown` not needed. |
| The Secret of Monkey Island | 1990 | 8086 | CLEAN | AdLib, MT-32 | Excellent | Excellent | Excellent | Excellent | SCUMM EGA/VGA. MT-32 score is superb. |
| Indiana Jones: Fate of Atlantis | 1992 | 286 | CLEAN | AdLib, SB, MT-32 | Good | Excellent | Excellent | Excellent | VGA SCUMM. CD "talkie" version → use CDROM, `requires_cd`. |
| Sid Meier's Civilization | 1991 | 286 | CLEAN | AdLib, MT-32 | Excellent | Excellent | Excellent | Excellent | Turn-based; CPU only affects big-map end-turn calc. |
| Dune II | 1992 | 286 | CLEAN | AdLib, SB, MT-32 | Good | Excellent | Excellent | Excellent | Sprite blitting slows with many units on SX. |
| Wolfenstein 3D | 1992 | 286 | XMS | AdLib, SB | Good | Excellent | Excellent | Excellent | Built to run on a 286; SX handles it well full-screen. |
| Alone in the Dark | 1992 | 386 | XMS | AdLib, SB, MT-32 | Playable-slow | Playable | Good | Excellent | Polygon fill hurts on SX 16-bit bus. Uses XMS/EMS. |
| SimCity (Classic) | 1989 | 8086 | CLEAN | PC Spkr, AdLib | Excellent | Excellent | Excellent | Excellent | EGA/VGA, mouse. Trivial CPU load. |
| Stunts (4D Driving) | 1990 | 286 | CLEAN | AdLib, PC Spkr | Playable-slow | Playable | Good | Good | Polygon 3D. On Pentium physics feel twitchy → `slowdown`. |
| Lemmings | 1991 | 8086 | CLEAN | AdLib, SB, MT-32 | Excellent | Excellent | Excellent | Excellent | Mouse required. Great everywhere. |
| X-COM: UFO Defense | 1994 | 386 | EMS | AdLib, SB, MT-32 | Playable-slow | Playable | Good | Excellent | Memory-hungry; uses EMS. Geoscape scroll slow on SX. |
| Ultima VII: The Black Gate | 1992 | 386 | CLEAN | AdLib, SB, MT-32 | Marginal | Playable-slow | Good | Excellent | **`no_emm`**: needs raw XMS, *no* EMM386/EMS. See §3.5. |
| Doom | 1993 | 386 | XMS | SB Pro, SB, GM | Unplayable | Playable-slow | Good | Excellent | DOS4GW extender (`dpmi`, `needs_xms`). SX = single-digit FPS. |
| Star Wars: TIE Fighter | 1994 | 386 | XMS | MT-32, GM, SB | Marginal | Playable-slow | Good | Excellent | DOS extender; iMUSE. MT-32 is the showcase. `dpmi`. |
| Warcraft: Orcs & Humans | 1994 | 386 | XMS | SB, SB Pro | Playable-slow | Playable | Good | Excellent | Real-mode RTS; unit-heavy scenes slow the SX. |
| Duke Nukem 3D | 1996 | 486 | XMS | SB Pro, SB, GM | Unplayable | Unplayable | Playable | Good | Build engine, DOS extender. `dpmi`,`needs_xms`,`vesa`(hi-res). |
| King's Quest V | 1990 | 286 | CLEAN | AdLib, SB, MT-32 | Good | Excellent | Excellent | Excellent | SCI1 VGA. CD talkie → CDROM, `requires_cd`. |
| Ultima Underworld | 1992 | 386 | EMS | AdLib, SB, MT-32 | Marginal | Playable-slow | Good | Excellent | Texture-mapped 3D; `needs_ems`. SX crawls at full window. |
| Master of Orion | 1993 | 286 | CLEAN | AdLib, SB, MT-32 | Excellent | Excellent | Excellent | Excellent | Turn-based 4X. CPU-light. |
| Syndicate | 1993 | 386 | XMS | SB, MT-32 | Playable-slow | Playable | Good | Excellent | Isometric real-mode; heavy scenes tax the SX. |
| The 7th Guest | 1993 | 486 | CDROM | SB, SB Pro | Unplayable | Unplayable | Playable | Good | FMV decode is the wall. 2× CD strongly advised. `requires_cd`. |
| Descent | 1995 | 486 | XMS | SB Pro, GM, GUS | Unplayable | Marginal | Playable-slow | Good | True 6DOF texture 3D. DOS4GW. `dpmi`,`needs_xms`,`vesa`. |
| Comanche: Maximum Overkill | 1992 | 386 | EMS | AdLib, SB, MT-32 | Marginal | Playable-slow | Good | Excellent | Voxel terrain engine; very CPU-bound. `needs_ems`. |
| Magic Carpet | 1994 | 486 | XMS | SB Pro, GM, GUS | Unplayable | Unplayable | Playable-slow | Good | DOS extender + texture 3D. `dpmi`,`needs_xms`,`vesa`. |

### Reading the table

- **386SX column is the honest one.** Notice the cliff: pre-1993 adventure,
  strategy, and platform titles are Good-to-Excellent, while every 1993+
  texture-mapped or extender-based action title drops to Marginal or Unplayable.
  That cliff is the 386SX's 16-bit bus and missing FPU, not a Castalia defect.
- **A Tier-A/B *class* can still earn a bad *SX verdict*.** Doom is fully
  *supported* (Tier B, XMS profile, `dpmi` handled) — it simply *isn't fast
  enough* on a 386SX. Support and performance are different axes.
- **"Playable-slow" is where judgment lives.** Ultima Underworld at Playable-slow
  on a 386DX means: shrink the view window, expect choppiness in open rooms, and
  it is genuinely finishable. That is not the same as Doom's Unplayable on SX.

---

## 3.4 The 386SX Reality

The 386SX is the reference floor, and this subsection exists so nobody is
surprised. The SX is a wonderful machine for the software of its own era and a
poor one for the software that arrived just after it.

### Why the SX is slower than its clock suggests

| Trait                       | Consequence                                                          |
|-----------------------------|---------------------------------------------------------------------|
| **16-bit external data bus** | Every 32-bit memory access is *two* bus cycles. Framebuffer fills, texture reads, and large copies pay double. This is the single biggest limiter. |
| **No/weak FPU**             | No 387 in most SX boxes. Any floating-point math (3D transforms, some physics) is emulated in software — dozens of times slower. |
| **Slower memory subsystem** | SX boards typically pair the 16-bit bus with slower RAM and no cache; effective throughput trails a 386DX at the same MHz. |
| **Real-mode overhead**      | Protected-mode extenders (DOS4GW) must switch modes and can run VCPI on top of a memory manager; the SX pays mode-switch and setup costs that add up in tight loops. |
| **VGA write bandwidth**     | Mode 13h is a byte-per-pixel blast to A000:0000; on a 16-bit bus at 16–25 MHz, filling 64,000 bytes/frame at 30+ FPS is right at the edge. |

The clock number lies: a 386SX/25 is *not* "a 386 at 25 MHz" for graphics-heavy
code. For a texture-mapped inner loop it behaves closer to a fast 286 with a math
handicap.

### Where the 386SX is marginal-to-unplayable

Texture-mapped and protected-mode action titles from **1993 onward**:

- **Doom, Descent, Duke Nukem 3D, Magic Carpet** — perspective-correct or
  affine texture mapping, per-pixel work, DOS extenders, FPU-friendly math. The
  SX runs them in the sense that they boot; it does not run them in the sense
  that they are fun. Single-digit to low-teens FPS in a shrunken window.
- **The 7th Guest** — not even about 3D: full-motion video decode from CD is a
  memory-bandwidth and integer-throughput wall the SX cannot clear.
- **Ultima Underworld, Comanche** — 1992 pioneers of texture/voxel 3D. The SX is
  *marginal*: playable only with the view window shrunk and patience applied.

The pattern is consistent: **the moment a title fills the screen per-frame with
computed pixels, the 16-bit bus and absent FPU decide the outcome, not Castalia.**

### Where the 386SX shines

The SX was designed for, and excels at, the 1988–1992 catalogue:

| Genre / era               | Why the SX is great                                                |
|---------------------------|--------------------------------------------------------------------|
| Point-and-click adventures | SCUMM/SCI redraw small dirty rectangles, not full frames. Monkey Island, Indy, King's Quest V: Excellent. |
| Turn-based strategy / 4X   | Civilization, Master of Orion, X-COM (tactical): CPU idles between turns; no framerate pressure. |
| 2D platformers             | Commander Keen, Prince of Persia: tile scroll and sprite blits sized for exactly this hardware. |
| RTS (early)                | Dune II, and Warcraft with modest unit counts: Good/Playable. |
| Puzzle / sim               | Lemmings, SimCity: mouse-driven, light per-frame work. Excellent. |
| Early 3D at low ambition   | Wolfenstein 3D (raycast, no floor/ceiling texturing, built for 286): Good on SX. |

**Rule of thumb for the SX:** if the game shipped **before Doom (Dec 1993)** and
isn't a polygon racer, it is very likely **Good or Excellent**. If it shipped
**after**, check the verdict table before promising anything.

---

## 3.5 What Breaks, and Why

Compatibility failures on real DOS hardware cluster into five recurring causes.
Castalia's job is to recognize each and steer the boot profile and GAMES.INI
flags around it.

### 3.5.1 Memory-manager-sensitive titles

Some games ship their own memory manager or make hard assumptions about the
system's memory map. Loading EMM386/JEMM386, UMBs, or an EMS page frame can
*break* them.

- **Ultima VII (The Black Gate / Serpent Isle)** is the canonical case. Its
  "Voodoo Memory Manager" (VMM) wants **raw XMS and expects no EMM386 resident**.
  Boot it under EMS/UMB and it fails or misbehaves. Castalia's answer: the
  `no_emm` flag forces the **CLEAN** profile (HIMEMX only, `DOS=HIGH`, no
  EMM386, no page frame). This is *why CLEAN exists as a first-class profile.*
- More generally, any title that pokes the memory map, needs a very large block
  of contiguous conventional memory, or dislikes UMBs is a candidate for
  `no_emm` and CLEAN.

**Castalia handling:** `no_emm=1` → CLEAN. `LAUNCH.EXE` refuses to run the title
from XMS/EMS/CDROM and prompts a reboot into CLEAN.

### 3.5.2 DPMI / DOS-extender (DOS4GW-class) titles

Protected-mode DOS games (Doom, Descent, Duke3D, TIE Fighter, Magic Carpet) run
on a **DOS extender** — most famously Rational Systems' DOS/4GW, bundled by
Watcom-compiled games. Extenders need a protected-mode host to cooperate with.

- With **no** memory manager, DOS4GW uses **raw XMS** directly — works under
  CLEAN if enough XMS is free.
- With **JEMM386** loaded (our XMS profile, `NOEMS`), DOS4GW uses **VCPI** to
  coexist — this is the normal, recommended path.
- **DPMI proper** (as some non-DOS4GW extenders want) is *not* provided by
  JEMM386; JEMM386 provides VCPI. Titles that demand a DPMI host specifically
  can need a DPMI shim, which is best-effort (Tier C).
- The classic failure is "**DOS/4GW error … not enough memory**": too little free
  XMS, or an EMM/host conflict. Fix by choosing XMS profile and freeing XMS.

**Castalia handling:** `dpmi=1` implies `needs_xms=1`; `LAUNCH.EXE` selects XMS,
verifies free XMS against the title's minimum, and warns instead of launching
blind.

### 3.5.3 Copy protection

Retro copy protection was hostile to *any* non-original environment, and some
schemes are hostile to fast or non-period hardware.

| Scheme                         | Failure mode                                     | Castalia stance |
|--------------------------------|--------------------------------------------------|-----------------|
| Manual / codewheel / lookup    | Asks for a word from the manual.                 | Works fine; user needs the docs. |
| Disk-based (bad sectors, laser holes) | Original floppy timing/format checks. | Works from original media on a real FDC; disk *images* often fail. |
| Fast-timing disk checks        | Check assumes a slow drive; fast CF/IDE trips it.| May need `slowdown`; best-effort. |
| CD "presence" checks           | Wants the original CD in the drive.              | Provide via CDROM profile + SHSUCDX; `requires_cd`. |
| Dongles (parallel-port)        | Hardware key on LPT.                             | Works if the dongle and its driver are present; not our concern. |

**Castalia handling:** we neither defeat nor endorse cracking protection. We make
sure the *environment* (drive timing, CD presence, LPT access) is period-correct
enough for legitimate media to validate. Genuinely exotic protection is Tier C.

### 3.5.4 Timing-loop speed sensitivity (fast-machine crashes)

Old games that time themselves with **CPU busy-loops** instead of the timer chip
run too fast — or crash — on hardware faster than the developer imagined.

- **"Runs too fast"**: input and animation are tied to raw CPU speed. Common on
  pre-1992 action titles when played on a Pentium. Stunts' physics get twitchy;
  some platformers become uncontrollable.
- **Divide-by-zero on boot**: the infamous class where a startup calibration loop
  overflows on very fast CPUs (the Turbo Pascal runtime-error pattern). This
  bites on high-MHz Pentiums and above, not on our 386/486 floor — but Castalia
  documents it because users *will* run these ROMs on faster boxes.

**Castalia handling:** the `slowdown` flag. On real hardware Castalia cannot lower
the CPU clock, but it can: disable the CPU/L1 cache for that session where the
chipset allows, load a cooperative delay TSR, or (documented) advise the user's
motherboard turbo switch. `slowdown` takes a level; higher = more aggressive.
This is mitigation, not emulation — we are honest that a Pentium is a Pentium.

### 3.5.5 CD audio vs CD data

A "CD-ROM game" can use the disc two entirely different ways, and confusing them
is a frequent support failure.

| Path         | What it is                                          | Needs                               |
|--------------|-----------------------------------------------------|-------------------------------------|
| **CD data**  | Program reads files/FMV/assets from the disc.       | ATAPI driver (UIDE) + SHSUCDX (MSCDEX-compatible) so `D:\` exists. |
| **CD audio** | Music is Red Book audio *tracks*, played by the drive itself over the analog/SPDIF cable to the sound card. | Drive audio cable connected; game issues MSCDEX/INT 2Fh play commands. |

- A game can need **data only** (7th Guest streams FMV from data sectors), **audio
  only** (some titles boot from floppy but play CD music), or **both**.
- Classic failure: files load fine but **there's no music** — the game wanted Red
  Book audio and the CD-audio cable to the sound card isn't connected, or the
  drive's audio playback isn't wired. That's a hardware cabling issue Castalia
  surfaces in HWINFO, not a driver bug.
- Another: **music plays but the game can't find its files** — SHSUCDX not loaded
  or wrong drive letter. Fixed by the CDROM profile.

**Castalia handling:** `requires_cd=1` selects the CDROM profile (UIDE +
SHSUCDX). `SETSOUND`/HWINFO checks and reports whether the drive's audio path is
present so "no CD music" is diagnosed, not guessed.

---

## 3.6 Compatibility Flags (GAMES.INI)

Per-game behavior is driven by `C:\CASTALIA\CFG\GAMES.INI`. Each title gets a
`[Section]`; Castalia tools (`LAUNCH.EXE`, `GAMECFG.EXE`, `MEMPROF.EXE`) read the
flags below to pick a boot profile, set `BLASTER`, and load helpers. Flags are
declarative: they describe the *need*; the tools choose the *mechanism*.

### Flag reference

| Flag         | Type   | Meaning                                              | Drives                                   |
|--------------|--------|------------------------------------------------------|------------------------------------------|
| `needs_ems`  | 0/1    | Title uses EMS (expanded) memory.                    | Selects **EMS** profile (JEMM386 + page frame E000). |
| `needs_xms`  | 0/1    | Title needs XMS (extended) memory ≥ its minimum.     | Selects **XMS** (or CLEAN if `no_emm`); checks free XMS. |
| `no_emm`     | 0/1    | Title must **not** have EMM386/EMS/UMB resident.     | Forces **CLEAN**; overrides `needs_ems`. |
| `slowdown`   | 0–3    | Timing-loop sensitive; apply slowdown level N.       | Cache-off / delay-TSR / turbo advisory.  |
| `requires_cd`| 0/1    | Needs CD present (data and/or Red Book audio).       | Selects **CDROM** (UIDE + SHSUCDX).      |
| `dpmi`       | 0/1    | Uses a DOS extender (DOS4GW-class); implies `needs_xms`. | Ensures VCPI host (XMS profile) + free XMS. |
| `vesa`       | 0/1    | Wants VESA VBE (usually 2.0) hi-res modes.           | Loads VBE TSR (UniVBE-class) if card lacks VBE 2.0. |

Additional descriptive keys used alongside the flags (not booleans):
`title=`, `exe=`, `path=`, `profile=` (explicit override), `blaster=` (the
`SET BLASTER` string), `xms_min=` (KB of XMS the title needs).

### Flag → profile resolution

`LAUNCH.EXE` resolves conflicts in this priority order:

1. `no_emm=1` → **CLEAN** (always wins; e.g. Ultima VII).
2. else `requires_cd=1` → **CDROM**.
3. else `needs_ems=1` → **EMS**.
4. else `dpmi=1` or `needs_xms=1` → **XMS**.
5. else → **CLEAN** (safest default).

`slowdown` and `vesa` are *modifiers*: they apply on top of whatever profile the
above selects (a slowdown TSR or a VBE TSR is loaded within that profile).

### Flag mapping for the table titles

| Game | needs_ems | needs_xms | no_emm | slowdown | requires_cd | dpmi | vesa | → Profile |
|------|:---------:|:---------:|:------:|:--------:|:-----------:|:----:|:----:|-----------|
| Prince of Persia | 0 | 0 | 0 | 0 | 0 | 0 | 0 | CLEAN |
| Commander Keen | 0 | 0 | 0 | 0 | 0 | 0 | 0 | CLEAN |
| Monkey Island | 0 | 0 | 0 | 0 | 0 | 0 | 0 | CLEAN |
| Indy: Fate of Atlantis | 0 | 0 | 0 | 0 | 0¹ | 0 | 0 | CLEAN (CDROM for talkie) |
| Civilization | 0 | 0 | 0 | 0 | 0 | 0 | 0 | CLEAN |
| Dune II | 0 | 0 | 0 | 0 | 0 | 0 | 0 | CLEAN |
| Wolfenstein 3D | 0 | 1 | 0 | 0 | 0 | 0 | 0 | XMS |
| Alone in the Dark | 1 | 1 | 0 | 0 | 0 | 0 | 0 | EMS/XMS |
| SimCity | 0 | 0 | 0 | 0 | 0 | 0 | 0 | CLEAN |
| Stunts | 0 | 0 | 0 | 2 | 0 | 0 | 0 | CLEAN + slowdown |
| Lemmings | 0 | 0 | 0 | 0 | 0 | 0 | 0 | CLEAN |
| X-COM: UFO Defense | 1 | 0 | 0 | 0 | 0 | 0 | 0 | EMS |
| **Ultima VII** | 0 | 1 | **1** | 0 | 0 | 0 | 0 | **CLEAN (forced)** |
| Doom | 0 | 1 | 0 | 0 | 0 | 1 | 0 | XMS |
| Star Wars: TIE Fighter | 0 | 1 | 0 | 0 | 0 | 1 | 0 | XMS |
| Warcraft: Orcs & Humans | 0 | 1 | 0 | 0 | 0 | 0 | 0 | XMS |
| Duke Nukem 3D | 0 | 1 | 0 | 0 | 0 | 1 | 1 | XMS |
| King's Quest V | 0 | 0 | 0 | 0 | 0¹ | 0 | 0 | CLEAN (CDROM for talkie) |
| Ultima Underworld | 1 | 0 | 0 | 0 | 0 | 0 | 0 | EMS |
| Master of Orion | 0 | 0 | 0 | 0 | 0 | 0 | 0 | CLEAN |
| Syndicate | 0 | 1 | 0 | 0 | 0 | 0 | 0 | XMS |
| The 7th Guest | 0 | 1 | 0 | 0 | 1 | 0 | 0 | CDROM |
| Descent | 0 | 1 | 0 | 0 | 0 | 1 | 1 | XMS |
| Comanche | 1 | 0 | 0 | 0 | 0 | 0 | 0 | EMS |
| Magic Carpet | 0 | 1 | 0 | 0 | 0 | 1 | 1 | XMS |

¹ *Floppy release needs no CD; the CD "talkie" re-release sets `requires_cd=1`.*

### Example `GAMES.INI` entries

```ini
; C:\CASTALIA\CFG\GAMES.INI  — excerpt
; Booleans are 0/1; slowdown is 0..3; xms_min is in KB.

[UltimaVII]
title=Ultima VII: The Black Gate
exe=U7.COM
path=C:\GAMES\ULTIMA7
no_emm=1
needs_xms=1
xms_min=2048
blaster=A220 I5 D1 H5 T6
; no_emm forces CLEAN: HIMEMX only, no EMM386, no page frame, no UMBs.

[Doom]
title=Doom
exe=DOOM.EXE
path=C:\GAMES\DOOM
dpmi=1
needs_xms=1
xms_min=4096
blaster=A220 I5 D1 H5 T4
; dpmi => XMS profile; DOS4GW rides VCPI from JEMM386 NOEMS.

[XCOM]
title=X-COM: UFO Defense
exe=UFO.BAT
path=C:\GAMES\XCOM
needs_ems=1
blaster=A220 I5 D1 H5 T3
; EMS profile: JEMM386 with FRAME=E000 page frame.

[Stunts]
title=Stunts
exe=STUNTS.EXE
path=C:\GAMES\STUNTS
slowdown=2
; CLEAN base; slowdown level 2 tames physics on 486/Pentium.

[Guest7]
title=The 7th Guest
exe=T7G.EXE
path=C:\GAMES\T7G
requires_cd=1
needs_xms=1
xms_min=2048
blaster=A220 I5 D1 H5 T4
; CDROM profile: UIDE + SHSUCDX; wants a 2x drive for FMV.

[DukeNukem3D]
title=Duke Nukem 3D
exe=DUKE3D.EXE
path=C:\GAMES\DUKE3D
dpmi=1
needs_xms=1
vesa=1
xms_min=8192
blaster=A220 I5 D1 H5 T4
; XMS profile + VBE TSR for hi-res; 486+ only in practice.
```

---

## 3.7 Compatibility Test Matrix (process)

Every build is validated against a fixed matrix before it can claim a tier.

| Layer                | Environment                          | Gate for                          |
|----------------------|--------------------------------------|-----------------------------------|
| Fast iteration       | **DOSBox-X**                         | Boot, menu, drivers, Tier A/B smoke. |
| Cycle-accuracy check | **86Box / PCem** (386SX/DX, 486)     | Timing, memory maps, Tier C behavior. |
| Final validation     | **Real 386SX hardware** + CF + VGA   | Ship gate for the flagship edition. |

Rules:

- **A tier claim is not valid until tested at the layer that gates it.** Tier A
  is re-checked every build; Tier C titles are checked on 86Box and, for the
  headline set, on real iron.
- **Emulator success is necessary, not sufficient.** DOSBox-X is forgiving about
  timing and memory; the 386SX is not. The verdict table reflects *real
  hardware* behavior, and where emulator and metal disagree, **metal wins.**
- **Every "Unplayable/Marginal" verdict is a measured claim,** not a guess: it
  reflects observed framerate/behavior on the reference CPU, and it stays honest
  even when it is unflattering.

---

## 3.8 Summary

- **Tier A must work** and is the reason Castalia exists: the 1988–1992 DOS
  catalogue on a 386SX, plus utilities, batch, FAT12/16, keyboard, VGA text,
  mouse, PC Speaker/AdLib. On the reference SX these are **Good to Excellent**.
- **Tier B should work** through the XMS/EMS/CDROM/WIN3X profiles: XMS/EMS games,
  CD-ROM, Sound Blaster, Windows 3.1 standard mode.
- **Tier C is best effort**: DOS extenders, DPMI, late 486/Pentium titles,
  unusual protection, WfW enhanced mode — supported with GAMES.INI tuning, honest
  about where the hardware runs out.
- **Tier D is out for 1.0**: no Win95 replacement, no protected-mode kernel, no
  preemptive multitasking, no LFN, no default networking, no USB, no modern
  filesystems.
- **The 386SX cliff is real and named**: pre-Doom titles fly; 1993+ texture-mapped
  and extender titles are marginal-to-unplayable, and we say so per title.
- **Failures have five named causes** (memory managers, extenders/DPMI, copy
  protection, timing loops, CD audio-vs-data), each mapped to a GAMES.INI flag and
  a boot profile.

Compatibility beats elegance. When in doubt, ship the profile that runs the game.
