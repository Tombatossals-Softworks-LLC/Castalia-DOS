# 4. Technical Architecture

CASTALIA DOS is a layered distribution. At the bottom is an unmodified (or
lightly configured) FreeDOS core; above it are Castalia drivers,
configuration, tools, and documentation. This document walks the whole tree
and, for each subsystem, states its **purpose**, the **recommended
implementation**, its **dependencies**, **compatibility concerns**, **386SX
performance concerns**, and the **future upgrade path**.

## System tree

```
CASTALIA DOS
│
├── Boot
│   ├── Boot sector (FreeDOS)                 loads KERNEL.SYS
│   ├── SYS installer                         makes a disk bootable
│   ├── Floppy boot (1.44M rescue/boot disk)
│   ├── Hard-drive / CF boot (FAT16, active)
│   └── Emergency recovery boot disk
│
├── Kernel
│   ├── FreeDOS KERNEL.SYS (GPLv2+)           real-mode DOS API
│   ├── Version reporting (SETVER per-program)
│   ├── MS-DOS 5.0/6.22-style behaviour
│   └── Future Castalia kernel fork (Phase 10)
│
├── Shell
│   ├── FreeCOM COMMAND.COM (GPLv2+)
│   ├── Branding / prompt / menu autostart
│   ├── Castalia Shell (future)
│   └── Command-compatibility requirements
│
├── Drivers
│   ├── HIMEMX (XMS)                          C:\DOS
│   ├── JEMM386 (EMM386-compatible; UMB/EMS)  C:\DOS
│   ├── CTMOUSE (serial + PS/2; GPL)          C:\CASTALIA\DRV
│   ├── UIDE + SHSUCDX (IDE/ATAPI CD)         C:\DOS / C:\CASTALIA\DRV
│   ├── KEYB (keyboard layouts)               C:\DOS
│   ├── ANSI (optional display)               C:\DOS
│   ├── Sound environment (SET BLASTER)       C:\CASTALIA\CFG\SOUND.BAT
│   └── Networking (out of 1.0 scope)
│
├── Utilities (FreeDOS base, curated)
│   ├── File tools (DIR/COPY/XCOPY/DELTREE)
│   ├── Disk tools (FDISK/FORMAT/SYS/CHKDSK)
│   ├── Memory tools (MEM)
│   ├── Diagnostics (see HWINFO)
│   ├── Text editor (EDIT / FreeDOS edit)
│   ├── Help (HELP.EXE + text pages)
│   └── Backup/restore (config-focused)
│
├── Castalia Original Tools (src/, MIT)
│   ├── CASTALIA.EXE     main menu
│   ├── LAUNCH.EXE       game launcher (GAMEVAULT.EXE alias)
│   ├── MEMPROF.EXE      memory-profile switcher
│   ├── SETSOUND.EXE     sound configuration
│   ├── HWINFO.EXE       hardware diagnostics
│   ├── CFGEDIT.EXE      config editor
│   ├── SAFEBOOT.EXE     rescue / safe boot
│   └── GAMECFG.EXE      per-game config
│
├── Profiles (CONFIG.SYS menu blocks + PROFILES.INI)
│   ├── CLEAN  XMS  EMS  CDROM
│   └── WIN3X  SAFE  DIAG  PROMPT  (+ CUSTOM)
│
└── Documentation (docs/, CC BY 4.0)
    ├── User guide         ├── Compatibility guide
    ├── Installation guide ├── Developer guide
    └── Troubleshooting guide
```

---

## Boot

### Boot sector & SYS installer
- **Purpose:** get from power-on to `KERNEL.SYS` on floppy, HDD, or CF.
- **Implementation:** use the FreeDOS boot sector and the FreeDOS `SYS`
  command to write the boot code and copy `KERNEL.SYS` + `COMMAND.COM`.
  Castalia's installer wraps `SYS` so users never invoke it directly.
- **Dependencies:** FreeDOS `SYS`, a FAT12 (floppy) or FAT16 (HDD/CF) volume.
- **Compatibility concerns:** some old BIOSes are picky about CF geometry and
  the active-partition flag; the installer validates both.
- **386SX performance:** irrelevant (one-time); boot time is dominated by BIOS
  POST and floppy seek, not our code.
- **Future path:** an optional Castalia boot sector with a small branded
  loader; strictly cosmetic and deferred.

### Floppy / HDD / CF boot & recovery disk
- **Purpose:** boot the normal system, and provide a self-contained rescue
  disk that boots even when the hard disk is unbootable.
- **Implementation:** the rescue floppy carries `KERNEL.SYS`, `COMMAND.COM`, a
  minimal `CONFIG.SYS`/`AUTOEXEC.BAT`, `SAFEBOOT.EXE`, `HWINFO.EXE`,
  `FDISK`, `FORMAT`, `SYS`, and `CFGEDIT.EXE`.
- **Dependencies:** all of the above fit on one 1.44 MB disk.
- **Compatibility concerns:** 720 KB vs 1.44 MB media; the installer detects
  capacity and warns.
- **386SX performance:** floppy I/O is slow (~30–60 KB/s); keep the rescue set
  minimal so it loads quickly.
- **Future path:** a CF "rescue partition" as an alternative to the floppy.

See [`BOOT.md`](BOOT.md) for the boot flow and the full config files.

---

## Kernel

- **Purpose:** provide the real-mode DOS API (`INT 21h` and friends), FAT
  file systems, memory allocation, and program loading.
- **Implementation:** ship the FreeDOS `KERNEL.SYS` as-is for 1.0. Configure,
  do not patch. If a patch is ever needed it stays in `third_party/` with
  source, under GPLv2+.
- **Version reporting:** FreeDOS reports version 7.x. Some programs demand a
  specific version; we ship `SETVER` to satisfy those *per program*. We do
  **not** globally spoof "6.22" — we advertise 6.22-*compatible behaviour*.
- **Dependencies:** a FreeDOS-compatible boot sector; `HIMEMX` for `DOS=HIGH`.
- **Compatibility concerns:** a small number of titles probe undocumented
  MS-DOS internals; those are Tier C best-effort (see
  [`COMPATIBILITY.md`](COMPATIBILITY.md)).
- **386SX performance:** the kernel's cost is fixed and tiny; the real lever
  is keeping it in the HMA (`DOS=HIGH`) to free conventional memory.
- **Future path:** Phase 10 — evaluate an original or forked kernel only if it
  provides a concrete compatibility or size win. Not a 1.0 concern.

---

## Shell

- **Purpose:** the command interpreter — `COMMAND.COM`, batch processing, the
  `CONFIG.SYS` menu, environment variables.
- **Implementation:** FreeCOM for 1.0. Castalia customises the prompt, the
  banner, environment setup, and the menu autostart via `AUTOEXEC.BAT`; it
  does not fork FreeCOM.
- **Command-compatibility requirements:** the batch features Castalia relies
  on must work exactly: the kernel's own multi-config directives
  (`MENU`/`MENUDEFAULT`/`MENUCOLOR` and `n?` line prefixes — *not* MS-DOS's
  `[MENU]`/`MENUITEM`, which this kernel does not implement), the `%CONFIG%`
  variable it exports as a digit, `GOTO %VAR%`, `IF ERRORLEVEL`, `IF EXIST`,
  `CALL`, and `SET`. FreeCOM and the FreeDOS kernel support all of these; see
  [`BOOT.md`](BOOT.md) for the syntax and why it is not the MS-DOS one.
- **Dependencies:** kernel; environment size set via `SHELL=... /E:1024`.
- **Compatibility concerns:** environment exhaustion (`Out of environment
  space`) if many variables are set — sized generously at 1024 bytes.
- **386SX performance:** `COMMAND.COM` resident size matters; a second shell
  via `system()` costs a few KB, which is why the launcher uses batch handoff
  instead of staying resident (see [`LAUNCHER.md`](LAUNCHER.md)).
- **Future path:** an optional **Castalia Shell** with nicer built-ins; kept
  strictly command-compatible with FreeCOM. Post-1.0.

---

## Drivers

| Driver | Purpose | License | Where | 386SX note |
|---|---|---|---|---|
| HIMEMX | XMS + HMA (`DOS=HIGH`) | open | `C:\DOS` | tiny; always worth loading |
| JEMM386 | UMB + optional EMS | open | `C:\DOS` | costs a little; enables load-high |
| CTMOUSE | serial + PS/2 mouse | GPL | `C:\CASTALIA\DRV` | load high; optional |
| UIDE | IDE/ATAPI CD-ROM | open/free | `C:\CASTALIA\DRV` | load high in CDROM profile |
| SHSUCDX | MSCDEX replacement | open/free | `C:\DOS` | load high; assigns drive letter |
| KEYB | keyboard layout | GPL | `C:\DOS` | tiny; locale-dependent |
| ANSI | ANSI display codes | open | `C:\DOS` | **not** loaded by default |

- **Recommended implementation:** load memory managers in `CONFIG.SYS`, load
  mouse/CD/`MSCDEX` in `AUTOEXEC.BAT` with `LH`/`DEVICEHIGH`, guarded by
  `IF EXIST` so a missing optional driver never stops the boot.
- **Compatibility concerns:** IRQ/DMA clashes (sound vs. network vs. CD),
  drive-letter collisions (`LASTDRIVE`), and games that dislike an EMS page
  frame — all handled by profile choice.
- **Sound environment:** not a driver but an environment contract
  (`SET BLASTER`); generated by `SETSOUND.EXE` into `SOUND.BAT`. See
  [`SOUND.md`](SOUND.md).
- **386SX performance:** every resident driver costs conventional memory; the
  profiles exist precisely to load only what the task needs.
- **Future path:** original Castalia CD and mouse helpers if the open ones
  ever fall short; networking is explicitly post-1.0.

---

## Utilities

- **Purpose:** the everyday DOS toolbox — files, disks, memory, editing, help.
- **Implementation:** curate the FreeDOS base utilities into `C:\DOS`. Do not
  reimplement what FreeDOS already does well (`DIR`, `COPY`, `XCOPY`,
  `FORMAT`, `FDISK`, `SYS`, `CHKDSK`, `MEM`, `EDIT`).
- **Dependencies:** kernel + shell.
- **Compatibility concerns:** keep MS-DOS-familiar command names and switches
  so muscle memory and old batch files work.
- **386SX performance:** these are transient programs; startup time matters
  more than anything, and FreeDOS utilities are lean.
- **Future path:** the Castalia **file manager** (`CASTFM.EXE`, 1.1) adds a
  visual layer over these; see [`FILE-MANAGER.md`](FILE-MANAGER.md).

---

## Castalia original tools

All original tools share two libraries in `src/common/`:

- **`INI` (`ini.c`/`ini.h`):** an allocation-free INI reader (one static
  buffer, in-place line splitting) used to read `CASTALIA.INI`,
  `PROFILES.INI`, and `GAMES.INI`.
- **`UI` (`ui.c`/`ui.h`):** an 80×25 text-mode toolkit (direct video-memory
  writes, BIOS cursor, `INT 16h` keyboard) implementing the Castalia palette.

| Tool | Purpose | Status | Notes |
|---|---|---|---|
| `CASTALIA.EXE` | Main menu / front end | prototype in `src/castalia` | exits errorlevel 10 to hand off to the launcher |
| `LAUNCH.EXE` | Game launcher | prototype in `src/launch` | batch handoff to free memory before a game |
| `MEMPROF.EXE` | Describe/switch profiles | prototype in `src/memprof` | reads `PROFILES.INI`; shows live memory; reboot to switch |
| `SETSOUND.EXE` | Sound config | prototype in `src/setsound` | writes `SOUND.BAT` from a chosen sound profile |
| `HWINFO.EXE` | Diagnostics | prototype in `src/hwinfo` | safe BIOS/DOS probes; see [`DIAGNOSTICS.md`](DIAGNOSTICS.md) |
| `CFGEDIT.EXE` | Config editor | prototype in `src/cfgedit` | line editor for `CONFIG.SYS`/`AUTOEXEC.BAT`; backs up on save |
| `SAFEBOOT.EXE` | Rescue | prototype in `src/safeboot` | restore/backup config or write a minimal bootable config |
| `GAMECFG.EXE` | Per-game config | prototype in `src/gamecfg` | add/edit/delete `GAMES.INI` entries; backs up on save |
| `CASTFM.EXE` | File manager | prototype in `src/castfm` | single-pane; list/view/run/copy/move/delete/mkdir/drive |
| `CASTMARK.EXE` | Inspector + benchmark | prototype in `src/castmark` | FNSTSW FPU probe; 5 marks; saved-score deltas; Castalia Index |
| `CASTCOPY.EXE` | Diskette rescue/transfer | prototype in `src/castcopy` | tag files on A:/B:; progress bars; read-back verify default-on |
| `CASTDOC.EXE` | Disk doctor (surface) | prototype in `src/castdoc` | INT 13h verify, live track map, diskette-vs-drive advice |
| `BANNER.EXE` | Animated boot banner | prototype in `src/banner` | castle build-up + shimmer; any key skips; `/Q` instant |
| `SNAKE.EXE` / `PUZZLE.EXE` / `ALMENA.EXE` / `MINAS.EXE` | Minigames | prototypes in `src/games` | registered in `GAMES.INI`; BIOS-tick paced |
| `HELP.EXE` | Help-topic reader | prototype in `src/help` | HELP.IDX index + pager; `HELP <topic>` prefix jump |
| `CASTEDIT.EXE` | General text editor | prototype in `src/castedit` | any file; `.BAK` on save; save-as; new files |

- **Dependencies:** `INI`, `UI`, the C runtime, and (for launching) the batch
  handoff convention in `AUTOEXEC.BAT`/`GAMES.BAT`.
- **Compatibility concerns:** none externally — they are ordinary DOS programs;
  their only "contract" with the system is the errorlevel handoff and the INI
  file locations.
- **386SX performance:** each obeys the rules in [`PERFORMANCE.md`](PERFORMANCE.md)
  — text mode, no TSRs, fixed buffers, single-pass INI parsing, start under
  ~2 seconds.
- **Future path:** grow the tool set (file manager, richer diagnostics) while
  holding the performance budget.

See [`LAUNCHER.md`](LAUNCHER.md) for the launcher and main-menu design, and the
source under `src/`.

---

## Profiles

- **Purpose:** map a single boot-menu choice to a coherent driver + memory
  layout tuned for one kind of workload.
- **Implementation:** each profile is a set of `n?`-prefixed `CONFIG.SYS`
  directives plus an `AUTOEXEC.BAT` branch, described for the user in
  `PROFILES.INI`.
- **Dependencies:** memory managers, drivers, the `%CONFIG%` variable.
- **Compatibility concerns:** the whole point — different games need different
  layouts (EMS vs. no EMS, CD vs. no CD, mouse vs. none).
- **386SX performance:** profiles are the primary tool for protecting
  conventional memory; CLEAN/XMS exist to give games the most RAM.
- **Future path:** a user-defined `CUSTOM` profile edited by `CFGEDIT.EXE`.

See [`MEMORY.md`](MEMORY.md) for every profile's `CONFIG.SYS`/`AUTOEXEC.BAT`.

---

## Documentation

- **Purpose:** make the system understandable — installation, profiles, games,
  sound, CD-ROM, memory, recovery, and legal.
- **Implementation:** Markdown in `docs/` (this bible) plus plain-text help
  pages installed to `C:\CASTALIA\HELP` and shown by `HELP.EXE`. Help pages
  fit an 80×25 screen. See [`HELP.md`](HELP.md).
- **Dependencies:** none at runtime beyond `HELP.EXE`.
- **386SX performance:** help pages are small text files; instant to display.
- **Future path:** a hyperlinked text help browser; still text-mode.

---

## How the layers cooperate at boot

```
POWER ON
   │  BIOS POST, load boot sector
   ▼
FreeDOS boot sector ──► KERNEL.SYS
   │  read CONFIG.SYS, show the Castalia boot MENU
   ▼
%CONFIG% = "1".."8"   (the kernel exports the menu DIGIT)
   │  run only the "n?" directives for that digit
   ▼
COMMAND.COM ──► AUTOEXEC.BAT
   │  digit ──► %CASTPROFILE% = (CLEAN|XMS|EMS|CDROM|WIN3X|DIAG|SAFE|PROMPT)
   │  set environment, load profile TSRs (mouse/CD/cache), set BLASTER
   ▼
GOTO %CASTPROFILE% branch
   │
   ├─ game profiles ──► CASTALIA.EXE (menu loop)
   │                       │ pick "Launch Games" ──► errorlevel 10
   │                       ▼
   │                    GAMES.BAT ──► LAUNCH.EXE ──► errorlevel 77
   │                       ▼
   │                    _RUNGAME.BAT ──► THE GAME (all free RAM)
   │
   ├─ DIAG ──► HWINFO.EXE ──► command prompt
   ├─ PROMPT ──► command prompt (no menu)
   └─ SAFE ──► rescue banner ──► command prompt
```

This is the spine of CASTALIA DOS: a faithful DOS core, a profile-driven boot,
and a set of lean text-mode tools that always get out of the way before a game
runs.
