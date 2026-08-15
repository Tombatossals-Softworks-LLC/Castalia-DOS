# CASTALIA DOS

**A practical, beautiful, game-focused DOS-compatible operating environment
for real 386SX, 386DX, 486, and early Pentium hardware.**

CASTALIA DOS is built on the [FreeDOS](https://www.freedos.org/) kernel and
shell and wraps them in an original suite of Castalia tools, boot profiles,
game launcher, installer, documentation, and branding. It behaves like DOS —
because DOS games expect DOS — while making everything *around* DOS better:
better boot menus, better memory profiles, a better game launcher, better
sound configuration, better diagnostics, and a polished 80×25 text-mode
experience.

> Flagship edition: **CASTALIA DOS 386SX Edition**
> Current design target: **1.0 "Tombatossals"**
> Foundation: **FreeDOS** (legal, open source) + original Castalia tooling

---

## What it is / what it is not

**It is:**

- A curated, bootable DOS environment optimised for DOS gaming on real
  retro hardware.
- A set of eight boot profiles (CLEAN / XMS / EMS / CDROM / WIN3X / DIAG /
  SAFE / PROMPT) that maximise conventional memory for the task at hand.
- Original text-mode tools: a game launcher, a main menu, sound and memory
  configuration, hardware diagnostics, and rescue utilities.
- FAT12/FAT16, VGA text UI, serial/PS-2 mouse, IDE/CompactFlash, floppy boot.

**It is not:**

- A clone of MS-DOS. It contains **no** Microsoft code, text, or branding.
- A protected-mode OS, a multitasker, or a Windows 95 replacement.
- A modern OS bolted onto old hardware. Compatibility beats elegance.

See [`docs/VISION.md`](docs/VISION.md) for the full rationale.

---

## Repository map

```
castalia-dos/
├── README.md                This file
├── LICENSE                  MIT (original Castalia code)
├── Makefile                 Open Watcom wmake build for the tools
├── LICENSES/                Third-party license texts + manifest
├── docs/                    The CASTALIA DOS technical bible (see below)
├── config/                  CONFIG.SYS, AUTOEXEC.BAT, *.INI, batch files
├── src/                     C source for the Castalia tools
│   ├── common/              INI parser + text-mode UI library
│   ├── launch/              LAUNCH.EXE  (game launcher)
│   ├── castalia/            CASTALIA.EXE (main menu)
│   ├── memprof/  setsound/  hwinfo/  setup/   (planned tools)
├── build/                   Build output (.obj / .exe)
├── dist/                    Release images (floppy/ZIP/ISO/CF)
├── floppy/                  Floppy-image staging
├── scripts/                 Build + image scripts
├── third_party/             FreeDOS + other GPL/open components (with source)
├── tools/                   Host-side helper tools
└── tests/                   Test assets and logs
```

---

## The technical bible (`docs/`)

| Document | Section | Covers |
|---|---|---|
| [`VISION.md`](docs/VISION.md) | 1 | Executive vision; why FreeDOS-based first |
| [`LICENSE-STRATEGY.md`](docs/LICENSE-STRATEGY.md) | 2 | Legal & licensing plan |
| [`COMPATIBILITY.md`](docs/COMPATIBILITY.md) | 3 | Compatibility tiers + per-game matrix |
| [`ARCHITECTURE.md`](docs/ARCHITECTURE.md) | 4 | Full system architecture |
| [`KERNEL.md`](docs/KERNEL.md) | — | The Castalia kernel: source mods + INT 2Fh identity API |
| [`BOOT.md`](docs/BOOT.md) | 5 | Boot process + example config files |
| [`MEMORY.md`](docs/MEMORY.md) | 6 | Memory management & profiles |
| [`LAUNCHER.md`](docs/LAUNCHER.md) | 7, 8 | Game launcher + main menu design |
| [`SOUND.md`](docs/SOUND.md) | 9 | Sound configuration (SETSOUND) |
| [`DIAGNOSTICS.md`](docs/DIAGNOSTICS.md) | 10 | Hardware diagnostics (HWINFO) |
| [`INSTALL.md`](docs/INSTALL.md) | 11 | Installer design + install guide |
| [`FILE-MANAGER.md`](docs/FILE-MANAGER.md) | 12 | File manager design (1.1) |
| [`HELP.md`](docs/HELP.md) | 13 | Documentation system + sample pages |
| [`DEVELOPER.md`](docs/DEVELOPER.md) | 14 | Developer toolchain |
| [`DISTRIBUTION.md`](docs/DISTRIBUTION.md) | 16 | Build & distribution plan |
| [`PERFORMANCE.md`](docs/PERFORMANCE.md) | 19 | 386SX performance budget |
| [`TESTING.md`](docs/TESTING.md) | 20 | Compatibility test matrix |
| [`ROADMAP.md`](docs/ROADMAP.md) | 21 | Phased roadmap |
| [`RISK-REGISTER.md`](docs/RISK-REGISTER.md) | 22 | Engineering risk register |
| [`BRANDING.md`](docs/BRANDING.md) | 23 | Product personality & branding |
| [`FUTURE-SHELL.md`](docs/FUTURE-SHELL.md) | 24 | Future graphical shell |
| [`BIBLE.md`](docs/BIBLE.md) | — | Master index + final recommendation |

---

## Building the tools

You need [Open Watcom C/C++ V2](https://open-watcom.github.io/) (free, cross-
hosts on Linux/Windows/macOS, produces 16-bit real-mode DOS executables).

```sh
# with the Open Watcom environment loaded (owsetenv.sh / setvars):
wmake            # builds launch, castalia, hwinfo, setsound, memprof (-> build/)
wmake clean      # removes build products
wmake hwinfo     # build just one tool
```

The Castalia Application Suite (see [`docs/APPS.md`](docs/APPS.md)) — all
building via `wmake`:

- **Core:** `CASTALIA.EXE` (menu), `LAUNCH.EXE` (launcher), `GAMECFG.EXE`
  (game database), `SETSOUND.EXE` (sound), `MEMPROF.EXE` (profiles),
  `SETUP.EXE` (installer), `SAFEBOOT.EXE` (rescue), `CFGEDIT.EXE` (config
  editor), `CASTEDIT.EXE` (text editor), `CASTFM.EXE` (file manager),
  `HWINFO.EXE` (diagnostics), `HELP.EXE` (help-topic reader),
  `CASTID.EXE` (kernel signature card — reads the Castalia kernel identity
  API live; see [`docs/KERNEL.md`](docs/KERNEL.md)).
- **Showcase:** `CASTMARK.EXE` (inspector + benchmark with animated bars and
  a real FNSTSW coprocessor probe), `CASTCOPY.EXE` (diskette rescue with
  verify), `CASTDOC.EXE` (read-only disk doctor with a live surface map).
- **Delight:** `BANNER.EXE` (animated castle boot banner), `SAVER.EXE`
  (a castle-at-night screensaver), `CDPLAYER.EXE` (a Red Book audio CD
  player driven through MSCDEX — pair it with the CDROM boot profile), and
  eight minigames — `SNAKE.EXE`, `PUZZLE.EXE`, `ALMENA.EXE`, `MINAS.EXE`,
  `SIEGE.EXE` (catapult duel vs. an adaptive CPU), `REVERSI.EXE` (Othello
  vs. a minimax opponent), `BARRELS.EXE` (fortress-cellar Sokoban), and
  `SOLITARE.EXE` (draw-one Klondike) — all registered in `GAMES.INI`.
  Every one of these is reachable from the `CASTALIA.EXE` main menu too.

See [`docs/DEVELOPER.md`](docs/DEVELOPER.md) for cross-build setup, memory
models, and the debugging/testing workflow (DOSBox-X → 86Box → real 386SX).

### Checks and tests

Local, fast (gcc only):

```sh
scripts/check.sh      # gate: C89 syntax of every tool, shell lint, repo sanity
scripts/test-unit.sh  # unit tests: the real INI module compiled natively (39 checks)
```

Full pipeline (what CI runs on every push —
[`.github/workflows/ci.yml`](.github/workflows/ci.yml)):

1. **check** — the repository gate.
2. **unit** — host unit tests against the real DOS sources.
3. **build** — the real 16-bit build: Open Watcom V2 compiles all 20 tools +
   the DOS smoke test; `scripts/verify-exes.sh` asserts every MZ executable;
   artifacts uploaded.
4. **e2e** — `scripts/fetch-payload.sh` downloads the FreeDOS payload
   (cached), `scripts/test-dos.sh` runs `SMOKE.EXE` *inside headless DOSBox*
   (INI parsing of the shipped configs under real DOS),
   `scripts/build-floppy.sh` produces the bootable image, and
   `scripts/test-boot.sh` **boots it** in DOSBox and asserts the
   `CASTALIA-BOOT-OK` marker written by `AUTOEXEC.BAT`. The image is uploaded
   as a CI artifact.

(Actions must be enabled for the repository; on a private repo that also
needs available Actions minutes.)

---

## Boot profiles at a glance

| # | Profile | For | Free conv. (≈386SX/4 MB) |
|---|---|---|---|
| 1 | CLEAN  | Cranky real-mode games | ~615 KB |
| 2 | XMS    | Most 1990–95 games (default) | ~628 KB |
| 3 | EMS    | EMS-aware games | ~600 KB |
| 4 | CDROM  | CD-ROM games | ~585 KB |
| 5 | WIN3X  | Windows 3.1 / WfW | ~590 KB |
| 6 | DIAG   | Diagnostics | — |
| 7 | SAFE   | Rescue | ~635 KB |
| 8 | PROMPT | Bare command prompt | ~615 KB |

Details and full `CONFIG.SYS`/`AUTOEXEC.BAT` in [`docs/MEMORY.md`](docs/MEMORY.md)
and [`docs/BOOT.md`](docs/BOOT.md); the working files are in [`config/`](config/).

---

## Legal

CASTALIA DOS is assembled from FreeDOS and other open/free components plus
original Castalia work. It reproduces **no** proprietary Microsoft material.
Original Castalia code is MIT-licensed; documentation is CC BY 4.0; modified
FreeDOS components remain GPLv2+ and ship with source in `third_party/`.
See [`docs/LICENSE-STRATEGY.md`](docs/LICENSE-STRATEGY.md) and
[`LICENSES/`](LICENSES/).

---

*Built for real machines, not screenshots.*
