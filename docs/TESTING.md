# 20. Compatibility Test Matrix

CASTALIA DOS — 386SX Edition · Technical Bible, Section 20

This section defines how CASTALIA DOS is validated for correctness and
compatibility across emulators and real retro hardware. Compatibility beats
elegance: a release ships only when the regression gate passes on both an
emulator and at least one piece of real 386-class hardware.

> **CI automation:** a four-stage pipeline runs on every push
> (`.github/workflows/ci.yml`): the repository gate (`scripts/check.sh`),
> host unit tests on the real INI, SPK, UI and CASTLINK modules
> (`scripts/test-unit.sh`), the real
> Open Watcom build of all tools with MZ verification
> (`scripts/verify-exes.sh`), and an E2E stage that fetches the FreeDOS
> payload (`scripts/fetch-payload.sh`), runs `SMOKE.EXE` inside headless
> DOSBox (`scripts/test-dos.sh`), builds the bootable floppy image, and
> **boots it**, asserting the `CASTALIA-BOOT-OK` marker written by the
> floppy `AUTOEXEC.BAT` (`scripts/test-boot.sh`). It also boots the image in
> QEMU and interrogates the running Castalia kernel over `INT 2Fh`
> (`scripts/test-kernel-api.sh`), which is the only automated check that
> exercises the kernel modifications as executing code. This automates the
> BOOT-01-class smoke; the matrix below (profiles, memory targets, games,
> real hardware) remains the release gate that CI cannot replace.

---

## 20.1 Test methodology

### Environments

Testing runs on four tiers, from fastest to most authoritative. A case is
"planned" until it has been executed at least once and its result recorded.

| Tier | Environment          | Purpose                                                              | Authority |
|------|----------------------|----------------------------------------------------------------------|-----------|
| 1    | **DOSBox-X**         | Fast iteration during development; scripting, memory dumps, logging. | Advisory  |
| 1b   | **QEMU**             | The automatable tier: boots our real `KERNEL.SYS` as a whole PC, in CI, headless. | Moderate |
| 2    | **86Box / PCem**     | Cycle-accurate 386SX/386DX/486 emulation; timing and chipset checks. | Strong    |
| 3    | **Real hardware**    | Final sign-off; genuine 386SX, IDE/CompactFlash, real sound cards.   | Final     |

### What each tier can actually settle

The tiers are not simply "more realistic" as you go down — they answer
*different* questions, and picking the wrong one wastes a day.

| Question | Settled by | Why not lower |
|---|---|---|
| Does the Castalia kernel answer `INT 2Fh AH=0CAh`? Is the OEM id `0CAh`? Does `CASTALIA=` reach the resident byte? | **QEMU** — `scripts/test-kernel-api.sh`, in CI | Pure software: the CPU runs the same bytes on any tier. |
| Does the image boot at all? | **QEMU / DOSBox-X** | Same. |
| Does `CASTLINK` really move a file over a null modem? | **QEMU**, two instances joined by a socket (`-serial tcp:...`) | The UART is emulated faithfully enough for the protocol; only signal timing at 115200 needs metal. |
| Does `UNDEL` find and rebuild a deleted file? | **QEMU** with a prepared FAT12 image | The FAT is data; no timing involved. |
| Does the shared UI toolkit paint the right cells? | **Host unit test** — `tests/unit/test_ui.c` | It is pure arithmetic on a 4 KB array. The test aims `UI.C` at an ordinary buffer (`UI_VRAM_BASE`) and diffs every primitive against the per-cell implementation the word-write version replaced, including the off-screen and degenerate cases. Nothing below this tier can *look* at a text screen, so an emulator would only prove the tool did not crash. |
| Does the PC speaker sound at the right FREQUENCIES? | **DOSBox-X**, `scripts/test-speaker.sh` | SDL's disk audio driver captures the emulated speaker to a file, and the tones are measured out of the samples. This reaches the 8253 divisor arithmetic, which the host tests cannot: there the port writes compile to no-ops. |
| Does the PC speaker sound *good* — volume, timbre, a real cone? | **86Box** (audible), metal for final | Measuring a frequency is not listening to it. |
| Does `CDPLAYER` read a table of contents and play the RIGHT track? | **DOSBox-X**, `scripts/test-cdaudio.sh` | It provides MSCDEX and mounts CUE/BIN with audio tracks, and SDL's disk audio driver captures what plays. A generated disc has a different pure tone per track, so measuring the output proves *which* track played — a wrong Red Book address reports success and sounds the wrong track. |
| Does `CDPLAYER` work through **UIDE + SHSUCDX** on a real drive? | **86Box**, then metal | DOSBox-X supplies its own MSCDEX, so the driver stack under the API is not the one a 386SX uses. Everything above the API is covered; the stack below it is not. |
| Does the CD-ROM **data** path work — `UIDE` finds the drive, `SHSUCDX` gives it a letter, `DIR D:` reads it? | **QEMU**, `-drive …,media=cdrom` with an ISO | ATAPI data commands are emulated faithfully; only the analogue audio path is missing. This is how the 20 MB XMS cache defect was found. |
| Does the eight-entry boot menu work, and does each entry load only its own drivers? | **QEMU**, boot the *installed* disk and screenshot each entry | It is pure `CONFIG.SYS` parsing. Booting only the rescue floppy — which has no menu — is what let a completely inert menu ship. |
| Does the video-adapter probe name MDA / CGA / EGA / MCGA / VGA correctly? | **DOSBox-X**, `scripts/test-video.sh` — 11 `machine=` types | Its `machine=` setting is the only source of adapters other than VGA available here; QEMU offers VGA and nothing else. |
| Is the `CASTMARK` index right (386SX/16 = 100)? | **Real hardware**; 86Box is the honest proxy | The whole measurement *is* real timing. Nothing else can settle it. |
| Sound Blaster IRQ/DMA, real IDE quirks, UMB layout | **Real hardware** | Emulators smooth over exactly these. |

So: **a VM is enough for most of it.** Everything on the first four rows is
automatable today. Only the last three genuinely need 86Box or metal, and of
those only the `CASTMARK` calibration is impossible to approximate.

### Reproducing the automated rig

Both tools install from ordinary package repositories; the Open Watcom build
is a tarball, no installer:

```sh
sudo apt-get install -y qemu-system-x86 mtools nasm dosbox shellcheck
curl -L -o ow.tar.xz \
  https://github.com/open-watcom/open-watcom-v2/releases/download/Current-build/ow-snapshot.tar.xz
sudo mkdir -p /opt/watcom && sudo tar -xJf ow.tar.xz -C /opt/watcom
export WATCOM=/opt/watcom PATH=/opt/watcom/binl:$PATH INCLUDE=/opt/watcom/h

wmake && wmake ktest                  # the suite plus the kernel probe
scripts/fetch-payload.sh              # FreeDOS payload (cached)
scripts/build-kernel.sh               # the Castalia kernel from patched source
scripts/build-floppy.sh --no-tools    # the 1.44 MB bootable image
scripts/test-kernel-api.sh            # boot it in QEMU and interrogate the kernel
```

The last step prints what the kernel says about itself and asserts it. A stock
FreeDOS kernel makes it fail (`CA00/CA01/CA02 ABSENT`, OEM id `FD`), which is
the check that the check is real.

Rationale for the tiers: DOSBox-X is convenient but forgiving — it will run
software a real 386SX cannot. QEMU is not forgiving in that way, but it is not
period-accurate either: it emulates a PC, not a *slow* PC. 86Box/PCem model real chipsets, DMA, PIC, PIT timing and slow
CPUs, so they catch most timing and memory problems. Real hardware is the only
authority for final sign-off, because emulators still smooth over bus timing,
CF-card quirks, sound-card IRQ/DMA edge cases and analog VGA behaviour.

Recommended emulator configurations:

- **DOSBox-X**: `machine=svga_s3`, `cputype=386`, `cycles=fixed 3000` to
  approximate a 386SX/16; raise cycles only to reproduce faster targets.
- **86Box**: 386SX profile — e.g. an Intel 80386SX @ 16 MHz board, 4 MB RAM,
  Tseng/Cirrus VGA, Sound Blaster 2.0 at A220 I5 D1, a small IDE/CF disk.
- **86Box**: 486 profile — 486DX2/66, 16 MB, PCI/VLB VGA, SB Pro or SB16.

### How results are recorded

Every planned case lives as one row in the matrix (§20.3). During a run the
tester fills the two run-time columns and leaves everything else as authored:

- **Actual result** — what actually happened, in one concise phrase. Left as
  `(pending)` until the case is run.
- **Pass/Fail** — `Pass`, `Fail`, or `Partial`. Left as `(pending)` until run.
- **Notes** — pre-authored guidance stays; the tester appends run specifics
  after a `—` (env build, measured KB, driver line, screenshot filename).

Each execution is logged to `tests/results/<date>-<tester>-<env>.md` as a copy
of the matrix with the two columns filled, plus a header block:

```
CASTALIA DOS test run
Build .......: 0.2 "Penyiscola" (rc2)
Date ........: 2026-07-09
Tester ......: dabellan
Environment .: 86Box 386SX/16, 4 MB, SB 2.0 (A220 I5 D1), 512 MB CF
Media .......: castalia-02rc2.img (SHA-256 in notes)
```

### Pass/Fail criteria

- **Pass** — observed behaviour matches *Expected result* exactly, with no
  hang, no data loss, no unhandled error, and no regression versus the last
  green build. Numeric targets (free conventional KB) must be met or exceeded.
- **Partial** — the core function works but a secondary aspect is off (e.g.
  game runs but music is silent; conventional memory is 3–5 KB below target).
  A Partial never satisfies the regression gate.
- **Fail** — behaviour differs from Expected, or any hang, corruption, crash,
  or data loss occurs. Any Fail in the gate subset blocks the release.

### How a tester reports a run

1. Copy the matrix into a dated results file and fill the header block.
2. Work top to bottom. For each case, perform the steps implied by
   *Software/Game* + *Profile* on the listed *Machine*, then fill *Actual
   result*, *Pass/Fail*, and append run detail to *Notes*.
3. For any Fail/Partial, capture evidence: a `MEM /C /P` dump for memory cases,
   a screenshot for boot/menu/UI cases, and the exact `SET`/driver line for
   memory, mouse, sound, and CD cases.
4. File findings as issues referencing the Test ID (e.g. `MEM-02`), and link
   the results file. The regression gate (§20.4) is re-checked before sign-off.

Reporting form (keep within 80 columns):

```
+----------------------------------------------------------------------------+
| Test ID ....: MEM-02                                                        |
| Machine ....: 86Box 386SX/16, 4 MB                                          |
| Profile ....: XMS                                                           |
| Expected ...: Free conventional 620-631 KB                                  |
| Actual .....: 627,344 bytes free (612 KB)  <-- fill during run             |
| Result .....: Pass / Fail / Partial        <-- fill during run             |
| Notes ......: MEM /C shows CTMOUSE + SETSOUND env in UMB; dump attached     |
+----------------------------------------------------------------------------+
```

Note on units: matrix targets are quoted in KB of free conventional memory as
reported by `MEM`. When recording, prefer the exact byte figure and convert
(1 KB = 1024 bytes) so a near-miss is not hidden by rounding.

---

## 20.2 Coverage map

The matrix exercises twelve categories. Every category has multiple cases and
at least one case on real hardware or 86Box before a release is signed off.

| Prefix   | Category                        | What it proves                                             |
|----------|---------------------------------|------------------------------------------------------------|
| BOOT-nn  | Boot tests                      | Every media type boots; every menu profile boots.          |
| INST-nn  | Install tests                   | Fresh/over-DOS install, backups, emergency disk.           |
| FS-nn    | File system tests               | FAT12/FAT16, paths, COPY/XCOPY/DELTREE.                     |
| MEM-nn   | Memory profile tests            | Free-conventional targets, EMS frame, UMB load-high.       |
| MOU-nn   | Mouse tests                     | Serial + PS/2 via CTMOUSE.                                  |
| SND-nn   | Sound variable tests            | `BLASTER` per profile; SETSOUND writes `SOUND.BAT`.        |
| CD-nn    | CD-ROM tests                    | UIDE + SHSUCDX mount, drive letter, data + CD-audio.       |
| GAME-nn  | Game launch tests               | Titles across CLEAN/XMS/EMS/CDROM.                          |
| BAT-nn   | Batch compatibility tests       | `n?` menu prefixes, `%CASTPROFILE%` branching, errorlevel. |
| WIN-nn   | Windows 3.1 tests               | WIN3X profile, standard vs enhanced mode.                  |
| HW-nn    | Real hardware tests             | 386SX boot, CF boot, timing-sensitive title.               |
| EMU-nn   | Emulator parity tests           | Same title behaves the same on DOSBox-X vs 86Box.          |

Machine shorthand used in the matrix: `386SX/16 4MB`, `386DX/40 8MB`,
`486DX2/66 16MB`, `Pentium 100 32MB`, `86Box 386SX`, `86Box 486`, `DOSBox-X`.
Profiles are the eight CONFIG.SYS menu entries. The kernel exports the chosen
one as `%CONFIG%` = the digit `"1"`…`"8"`; `AUTOEXEC.BAT` maps it to the name
used throughout this document and in `%CASTPROFILE%`: CLEAN, XMS, EMS, CDROM,
WIN3X, DIAG, SAFE, PROMPT.

---

## 20.3 Test matrix

Wide table; render or scroll horizontally. Free-conventional targets follow the
canonical profile design: CLEAN ~615 KB, XMS 620–631 KB, EMS ~600–610 KB (page
frame costs 64 KB), CDROM ~590 KB after UIDE + SHSUCDX load high, SAFE runs DOS
low with no drivers.

| Test ID  | Category | Machine        | CPU        | RAM  | Profile | Software/Game            | Expected result                                                        | Actual result | Pass/Fail | Notes |
|----------|----------|----------------|------------|------|---------|--------------------------|------------------------------------------------------------------------|---------------|-----------|-------|
| BOOT-01  | Boot     | 386SX/16 4MB   | 386SX/16   | 4MB  | CLEAN   | Boot floppy 1.44MB       | KERNEL.SYS loads from FAT12 floppy; Castalia boot menu drawn in 80x25.  | (pending)     | (pending) | Blue background, amber highlights; 8 menu entries in order CLEAN..PROMPT. |
| BOOT-02  | Boot     | 486DX2/66 16MB | 486DX2/66  | 16MB | XMS     | Installed HDD (IDE)      | Boots from FAT16 C:; menu appears; XMS is the highlighted default.       | (pending)     | (pending) | Confirms master boot record + partition boot handoff to KERNEL.SYS. |
| BOOT-03  | Boot     | 386SX/16 4MB   | 386SX/16   | 4MB  | CLEAN   | CompactFlash (IDE mode)  | CF presents as fixed disk; boots identically to spinning IDE.            | (pending)     | (pending) | CF in true IDE mode, LBA off if BIOS is CHS-only; note geometry. |
| BOOT-04  | Boot     | 86Box 386SX    | 386SX/16   | 4MB  | CLEAN   | Menu selection CLEAN     | CLEAN block runs: HIMEMX + DOS=HIGH only, no EMM386/UMB/TSR.             | (pending)     | (pending) | AUTOEXEC branch for CLEAN reaches menu/prompt without error. — real 386SX 2026-10-01: booted to the menu on CLEAN, EMS absent; Pass (tests/results/2026-10-01-dabellan-386sx-real.md). |
| BOOT-05  | Boot     | 86Box 386SX    | 386SX/16   | 4MB  | XMS     | Menu selection XMS       | XMS block runs: HIMEMX + JEMM386 NOEMS + DOS=HIGH,UMB.                    | (pending)     | (pending) | No EMS page frame; drivers load high; default gaming profile. |
| BOOT-06  | Boot     | 86Box 386SX    | 386SX/16   | 4MB  | EMS     | Menu selection EMS       | EMS block runs: HIMEMX + JEMM386 with FRAME=E000 + DOS=HIGH,UMB.         | (pending)     | (pending) | Page frame reserved at E000; EMS + UMB both active. — real 386SX 2026-10-01: XMS 3.00 + EMS present; frame address and UMBs not captured (tests/results/2026-10-01-dabellan-386sx-real.md). |
| BOOT-07  | Boot     | 86Box 486      | 486DX2/66  | 16MB | CDROM   | Menu selection CDROM     | CDROM block runs: EMS base + UIDE.SYS + SHSUCDX high; CD drive letter.    | (pending)     | (pending) | Verify D: assigned; sound env set for CD games. |
| BOOT-08  | Boot     | 86Box 486      | 486DX2/66  | 16MB | WIN3X   | Menu selection WIN3X     | WIN3X block runs: HIMEMX + JEMM386 (EMS on) + DOS=HIGH,UMB + SMARTDRV.    | (pending)     | (pending) | Mouse + SMARTDRV loaded; ready for WIN.COM. |
| BOOT-09  | Boot     | 86Box 386SX    | 386SX/16   | 4MB  | DIAG    | Menu selection DIAG      | CLEAN base boots; AUTOEXEC runs HWINFO.EXE and shows system report.      | (pending)     | (pending) | HWINFO must detect CPU=386SX, RAM, VGA, mouse, sound ports. |
| BOOT-10  | Boot     | 86Box 386SX    | 386SX/16   | 4MB  | SAFE    | Menu selection SAFE      | Bare KERNEL.SYS + COMMAND.COM, no drivers, DOS low; usable prompt.       | (pending)     | (pending) | Rescue path; must succeed even if drivers are broken. |
| BOOT-11  | Boot     | 86Box 386SX    | 386SX/16   | 4MB  | PROMPT  | Menu selection PROMPT    | CLEAN base; AUTOEXEC drops straight to bare C:\> with no menu.           | (pending)     | (pending) | CASTALIA.EXE must NOT auto-launch in this profile. |
| BOOT-12  | Boot     | DOSBox-X       | 386        | 4MB  | XMS     | Menu default timeout     | With no keypress, menu times out and boots the default (XMS).            | (pending)     | (pending) | Confirm MENUDEFAULT + timeout in CONFIG.SYS; note seconds. |
| INST-01  | Install  | 486DX2/66 16MB | 486DX2/66  | 16MB | (n/a)   | Fresh install, blank HDD | Installer partitions + FAT16-formats C:, lays out C:\DOS, C:\CASTALIA.    | (pending)     | (pending) | Verify directory layout per bible; PATH includes C:\CASTALIA\BIN. |
| INST-02  | Install  | 386DX/40 8MB   | 386DX/40   | 8MB  | (n/a)   | Install over MS-DOS 6.22 | Existing DOS + user files preserved; Castalia boot files installed.      | (pending)     | (pending) | Must not overwrite user data; SETVER kept for programs needing it. |
| INST-03  | Install  | 386DX/40 8MB   | 386DX/40   | 8MB  | (n/a)   | Config backup on install | Original CONFIG.SYS + AUTOEXEC.BAT copied to C:\CASTALIA\BACKUP.          | (pending)     | (pending) | Timestamped copies; restore documented in help. |
| INST-04  | Install  | 386SX/16 4MB   | 386SX/16   | 4MB  | (n/a)   | Emergency boot disk make | Installer writes a bootable FAT12 rescue floppy (SAFEBOOT + tools).      | (pending)     | (pending) | Disk boots to SAFE-equivalent prompt with SAFEBOOT.EXE present. |
| INST-05  | Install  | 386SX/16 4MB   | 386SX/16   | 4MB  | (n/a)   | Install to CompactFlash  | CF installs and boots like an HDD; no write errors on FAT16.             | (pending)     | (pending) | Note CF write endurance; installer avoids needless rewrites. |
| INST-06  | Install  | 486DX2/66 16MB | 486DX2/66  | 16MB | (n/a)   | Repair/reinstall         | Re-running installer refreshes system files but keeps C:\GAMES + INI.    | (pending)     | (pending) | CASTALIA.INI/PROFILES.INI/GAMES.INI must survive repair. |
| FS-01    | FS       | 386SX/16 4MB   | 386SX/16   | 4MB  | CLEAN   | FORMAT A: FAT12 floppy   | 1.44MB floppy formats FAT12; files written and read back identical.      | (pending)     | (pending) | Verify byte-identical readback of a 100 KB file. |
| FS-02    | FS       | 486DX2/66 16MB | 486DX2/66  | 16MB | CLEAN   | FDISK + FORMAT FAT16 HDD | Primary partition created, FAT16-formatted, bootable, chkdsk clean.      | (pending)     | (pending) | Partition <= 2 GB for FAT16; note cluster size. |
| FS-03    | FS       | 486DX2/66 16MB | 486DX2/66  | 16MB | CLEAN   | Deep 8.3 path handling   | Nested dirs to full DOS path length created/traversed without error.     | (pending)     | (pending) | 8.3 names only; path near 63-char limit handled gracefully. |
| FS-04    | FS       | 386DX/40 8MB   | 386DX/40   | 8MB  | CLEAN   | COPY single file         | COPY duplicates a file to another dir; size/attrs preserved.             | (pending)     | (pending) | Compare with FC; note read-only/hidden attribute behaviour. |
| FS-05    | FS       | 386DX/40 8MB   | 386DX/40   | 8MB  | CLEAN   | XCOPY /S subtree         | XCOPY /S/E copies a directory tree including empty subdirs.              | (pending)     | (pending) | Verify count of files matches source; note /E for empty dirs. |
| FS-06    | FS       | 386DX/40 8MB   | 386DX/40   | 8MB  | CLEAN   | DELTREE removes tree     | DELTREE deletes a populated directory tree in one command.              | (pending)     | (pending) | Confirm no orphaned clusters via chkdsk afterwards. |
| FS-07    | FS       | 486DX2/66 16MB | 486DX2/66  | 16MB | CLEAN   | Large file on FAT16      | A 40 MB file writes and reads back intact on FAT16.                      | (pending)     | (pending) | Exercises >32 MB handling; verify with checksum. |
| MEM-01   | Memory   | 86Box 386SX    | 386SX/16   | 4MB  | CLEAN   | MEM /C /P                | Free conventional >= ~615 KB; HIMEMX + DOS=HIGH only, no UMBs.           | (pending)     | (pending) | No EMM386, no TSRs; record exact byte figure. |
| MEM-02   | Memory   | 86Box 386SX    | 386SX/16   | 4MB  | XMS     | MEM /C /P                | Free conventional 620-631 KB; UMBs on, no EMS page frame.               | (pending)     | (pending) | Highest conventional profile; mouse+sound env high. |
| MEM-03   | Memory   | 86Box 386SX    | 386SX/16   | 4MB  | EMS     | MEM /C /P                | Free conventional ~600-610 KB; ~64 KB consumed by page frame.           | (pending)     | (pending) | Lower than XMS by design; EMS pool must be usable. |
| MEM-04   | Memory   | 86Box 486      | 486DX2/66  | 16MB | CDROM   | MEM /C /P                | Free conventional >= ~590 KB after UIDE.SYS + SHSUCDX load high.         | (pending)     | (pending) | Both CD drivers must be in UMB, not conventional. |
| MEM-05   | Memory   | 86Box 386SX    | 386SX/16   | 4MB  | EMS     | MEM (EMS report)         | EMS present; page frame reported at E000; LIM 4.0 available.            | (pending)     | (pending) | Confirm FRAME=E000; note total EMS KB reported. |
| MEM-06   | Memory   | 86Box 386SX    | 386SX/16   | 4MB  | XMS     | MEM (EMS report)         | No EMS present (JEMM386 NOEMS); XMS present and usable.                  | (pending)     | (pending) | Games needing EMS should be told to use the EMS profile. |
| MEM-07   | Memory   | 86Box 486      | 486DX2/66  | 16MB | XMS     | MEM /C load-high check   | CTMOUSE and sound env verified resident in UMB, not conventional.       | (pending)     | (pending) | Any driver in conventional here is a Fail; list region. |
| MEM-08   | Memory   | 86Box 386SX    | 386SX/16   | 4MB  | SAFE    | MEM /C                   | DOS resident low; no HIMEMX/EMM386/UMB; minimal footprint.              | (pending)     | (pending) | Confirms SAFE is truly bare for rescue scenarios. |
| MOU-01   | Mouse    | 386DX/40 8MB   | 386DX/40   | 8MB  | XMS     | CTMOUSE serial COM1      | CTMOUSE detects serial mouse on COM1; INT 33h cursor moves.             | (pending)     | (pending) | Note IRQ4/COM1 base 3F8; test 2-button motion + clicks. |
| MOU-02   | Mouse    | 486DX2/66 16MB | 486DX2/66  | 16MB | XMS     | CTMOUSE PS/2             | CTMOUSE detects PS/2 mouse (IRQ12); INT 33h cursor moves.               | (pending)     | (pending) | GPL driver (see LICENSES/MANIFEST.md); confirm loaded high in UMB. |
| MOU-03   | Mouse    | 486DX2/66 16MB | 486DX2/66  | 16MB | WIN3X   | Mouse inside application | Mouse usable in a mouse-driven DOS menu and in Windows 3.1.             | (pending)     | (pending) | Cross-check with WIN-02; verify no cursor lag. |
| SND-01   | Sound    | 86Box 386SX    | 386SX/16   | 4MB  | XMS     | SET BLASTER check        | Env reads SET BLASTER=A220 I5 D1 H5 T4 (adjusted per detected card).     | (pending)     | (pending) | A=port,I=IRQ,D=8-bit DMA,H=16-bit DMA,T=type; note card. |
| SND-02   | Sound    | 386DX/40 8MB   | 386DX/40   | 8MB  | XMS     | SETSOUND.EXE run         | SETSOUND writes SOUND.BAT with chosen card's BLASTER + env lines.       | (pending)     | (pending) | Re-running SETSOUND rewrites SOUND.BAT idempotently. |
| SND-03   | Sound    | 86Box 386SX    | 386SX/16   | 4MB  | XMS     | AdLib music playback     | FM/OPL music plays at port 388h; melody audible and in tune.            | (pending)     | (pending) | AdLib profile; independent of digital DMA path. |
| SND-04   | Sound    | 86Box 486      | 486DX2/66  | 16MB | XMS     | SB digital playback      | 8-bit PCM via DMA channel 1 plays; no stutter or DMA hang.              | (pending)     | (pending) | Uses D1 from BLASTER; verify IRQ5 acknowledge. |
| SND-05   | Sound    | 386SX/16 4MB   | 386SX/16   | 4MB  | CLEAN   | PC Speaker fallback      | With PC Speaker profile, beeps/effects play via timer; no card needed.  | (pending)     | (pending) | Confirms graceful path on machines without a sound card. |
| SND-06   | Sound    | 386SX/16 4MB   | 386SX/16   | 4MB  | CLEAN   | None (silent) profile    | With sound None, no BLASTER set; games detect "no sound" cleanly.       | (pending)     | (pending) | SETSOUND must be able to disable audio entirely. |
| CD-01    | CD-ROM   | QEMU + 486     | 486DX2/66  | 16MB | CDROM   | UIDE + SHSUCDX mount     | ATAPI CD detected by UIDE.SYS; SHSUCDX mounts it as drive D:.           | QEMU: PASS    | (pending) | MSCDEX replacement; UIDE runs /N1 /N3 (no XMS cache) so it fits a 4 MB machine. |
| CD-02    | CD-ROM   | QEMU + 486     | 486DX2/66  | 16MB | CDROM   | Read data CD directory   | DIR D: lists ISO9660 contents; files copy off the disc intact.         | QEMU: DIR OK  | (pending) | Directory listing verified; copying off the disc still to check on metal. |
| CD-03    | CD-ROM   | Pentium 100 32MB | Pentium 100 | 32MB | CDROM  | CD-audio playback        | Red Book audio track plays via CD-audio control; through line-out.      | (pending)     | (pending) | Needs analog CD-audio path or SB mixer; note wiring. |
| CD-04    | CD-ROM   | 86Box 486      | 486DX2/66  | 16MB | CDROM   | No disc inserted         | With no disc, D: exists but reports not-ready gracefully; no hang.      | (pending)     | (pending) | Critical-error handler must offer Retry/Fail cleanly. |
| GAME-01  | Game     | 486DX2/66 16MB | 486DX2/66  | 16MB | XMS     | Doom (shareware ep.1)    | Loads under XMS; playable frame rate; keyboard + sound work.            | (pending)     | (pending) | Uses DOS/4GW-style extender + XMS; note fps feel. |
| GAME-02  | Game     | 386SX/16 4MB   | 386SX/16   | 4MB  | XMS     | Wolfenstein 3D           | Runs on 386SX; playable, sound via SB/AdLib; no memory error.          | (pending)     | (pending) | Honest note: fluid on 386SX; good XMS baseline. |
| GAME-03  | Game     | 386SX/16 4MB   | 386SX/16   | 4MB  | CLEAN   | Commander Keen           | Real-mode EGA title runs under CLEAN with max conventional.            | (pending)     | (pending) | No memory manager needed; validates CLEAN for cranky titles. |
| GAME-04  | Game     | 486DX2/66 16MB | 486DX2/66  | 16MB | EMS     | Ultima Underworld        | Detects EMS; loads and runs; mouse look works.                          | (pending)     | (pending) | EMS-aware title; would fail under XMS(NOEMS) profile. |
| GAME-05  | Game     | 386DX/40 8MB   | 386DX/40   | 8MB  | EMS     | Sierra AGI/SCI title     | EMS-aware Sierra adventure loads; music + mouse functional.            | (pending)     | (pending) | Confirms EMS profile serves Sierra as documented. |
| GAME-06  | Game     | Pentium 100 32MB | Pentium 100 | 32MB | CDROM  | CD adventure (data disc) | Launches from D:; reads assets from CD; sound + video play.            | (pending)     | (pending) | End-to-end CDROM profile game path. |
| GAME-07  | Game     | 386SX/16 4MB   | 386SX/16   | 4MB  | CLEAN   | Prince of Persia         | Runs under CLEAN; timing feels correct (not too fast) on 386SX.        | (pending)     | (pending) | Timing-sensitive; cross-check HW-03 on real 386SX. |
| GAME-08  | Game     | 486DX2/66 16MB | 486DX2/66  | 16MB | XMS     | Launch via LAUNCH.EXE    | LAUNCH.EXE reads GAMES.INI, starts title, returns to menu on exit.     | (pending)     | (pending) | Validates GAMEVAULT launcher + per-game GAMECFG handoff. |
| BAT-01   | Batch    | QEMU + 386SX   | 386SX/16   | 4MB  | (all)   | CONFIG.SYS menu entries  | All 8 entries appear and are selectable; each sets %CONFIG% to its digit; only that entry's drivers load. | QEMU: PASS | (pending) | Entry order CLEAN,XMS,EMS,CDROM,WIN3X,DIAG,SAFE,PROMPT; verified by screenshot per entry. |
| BAT-02   | Batch    | QEMU + 386SX   | 386SX/16   | 4MB  | (all)   | AUTOEXEC profile branch  | AUTOEXEC.BAT maps the %CONFIG% digit to %CASTPROFILE% and branches to the right section. | QEMU: PASS | (pending) | GOTO %CASTPROFILE%; a :LABEL exists for every profile. |
| BAT-03   | Batch    | 486DX2/66 16MB | 486DX2/66  | 16MB | XMS     | errorlevel launcher      | LAUNCH.EXE exit code drives batch: clean return vs error path.        | (pending)     | (pending) | IF ERRORLEVEL ladder returns control to CASTALIA.EXE. |
| WIN-01   | Windows  | 386SX/16 4MB   | 386SX/16   | 4MB  | WIN3X   | Windows 3.1 standard     | WIN /S starts standard mode; usable on 386SX with 4 MB.               | (pending)     | (pending) | 386SX 4MB is standard-mode territory; note responsiveness. |
| WIN-02   | Windows  | 486DX2/66 16MB | 486DX2/66  | 16MB | WIN3X   | Windows 3.1 enhanced     | WIN starts 386-enhanced mode; mouse works; SMARTDRV active if supplied.           | (pending)     | (pending) | Enhanced mode wants a 386+ and more RAM; 486/16MB ideal. |
| WIN-03   | Windows  | 486DX2/66 16MB | 486DX2/66  | 16MB | WIN3X   | User SMARTDRV + Windows  | SMARTDRV cache present; disk performance improved; no corruption.     | (pending)     | (pending) | Verify write-cache flush on exit; no lost clusters. |
| HW-01    | Hardware | 386SX/16 4MB   | 386SX/16   | 4MB  | CLEAN   | Real 386SX floppy boot   | Genuine 386SX boots the install floppy to the Castalia menu.         | (pending)     | (pending) | Final-authority boot check on real silicon + real VGA. |
| HW-02    | Hardware | 386SX/16 4MB   | 386SX/16   | 4MB  | XMS     | Real CF boot             | CF card in IDE adapter boots to menu; default XMS runs.              | (pending)     | (pending) | Note CF brand/geometry; watch for BIOS LBA limits. — real 386SX 2026-10-01: installed C: boots to the menu on CLEAN and EMS; Partial, XMS not run, disk type not recorded (tests/results/2026-10-01-dabellan-386sx-real.md). |
| HW-03    | Hardware | 386SX/16 4MB   | 386SX/16   | 4MB  | CLEAN   | Timing-sensitive title   | Prince of Persia runs at correct speed on real 386SX; not too fast.  | (pending)     | (pending) | Emulators may mis-time; real HW is the arbiter here. |
| EMU-01   | Parity   | DOSBox-X       | 386        | 4MB  | XMS     | Doom (shareware ep.1)    | Behaviour (load, input, sound) matches 86Box 386SX run of same title. | (pending)     | (pending) | Cross-reference GAME-01; note any divergence. |
| EMU-02   | Parity   | 86Box 386SX    | 386SX/16   | 4MB  | (all)   | Boot menu + profiles     | Menu layout and profile boots identical to DOSBox-X.                  | (pending)     | (pending) | Any profile that diverges is a config portability bug. |
| EMU-03   | Parity   | 86Box 386SX    | 386SX/16   | 4MB  | XMS     | MEM /C free-conventional | Free-conventional within a few KB of the DOSBox-X XMS figure.        | (pending)     | (pending) | Large deltas indicate emulator UMB/region differences. |

---

## 20.4 Regression gate

Before any release (Almenara through Montornes and beyond), the following
minimal subset MUST pass on **at least one emulator (86Box preferred)** and on
**at least one piece of real 386-class hardware**. A `Fail` or `Partial` on any
gate case blocks the release until fixed or explicitly waived by the maintainer
with a written note in the results file.

| Gate case | Why it is a gate                                                        |
|-----------|-------------------------------------------------------------------------|
| BOOT-01   | If the floppy will not boot, nothing else matters.                       |
| BOOT-10   | SAFE mode is the rescue path; it must always come up.                    |
| BOOT-11   | PROMPT must reach a bare prompt without auto-launching the menu.         |
| INST-01   | Fresh install must lay out the canonical directory tree and boot files.  |
| INST-03   | User CONFIG.SYS/AUTOEXEC.BAT must be backed up before any modification.  |
| INST-04   | An emergency boot disk must exist for recovery.                          |
| FS-02     | FAT16 HDD must format, boot, and pass chkdsk.                            |
| MEM-01    | CLEAN must meet its ~615 KB conventional target.                         |
| MEM-02    | XMS (default gaming) must meet its 620-631 KB target.                    |
| MEM-07    | Drivers must load high; any driver stuck in conventional fails the gate. |
| MOU-01    | Mouse (serial via CTMOUSE) is baseline input for menus and games.        |
| SND-02    | SETSOUND must correctly write SOUND.BAT with the BLASTER line.           |
| CD-01     | CDROM profile must mount a CD as a drive letter (UIDE + SHSUCDX).        |
| GAME-02   | A representative game must run on the flagship 386SX target.             |
| BAT-02    | AUTOEXEC must branch correctly to every profile's section.               |
| HW-01     | Real 386SX must boot to the menu — final sign-off cannot be waived.      |

Gate procedure:

1. Run the full matrix (§20.3) on 86Box; log to `tests/results/`.
2. Run the gate subset on real 386SX hardware; log a second results file.
3. If every gate case is `Pass` on both an emulator and real hardware, tag the
   build as gate-green in the results header and proceed to release packaging.
4. Any `Fail`/`Partial` in the gate: file an issue by Test ID, fix, and re-run
   at minimum the affected case plus every gate case it could have regressed.

Honest limitation note: on a 386SX/16 with 4 MB, some 1994+ titles and Windows
3.1 enhanced mode are out of reach; those cases are validated on 386DX/486/
Pentium rows and are not part of the 386SX gate. The gate certifies that the
flagship 386SX Edition boots, installs safely, hits its memory targets, mounts
CDs, plays sound, and runs a representative game — on real hardware.
