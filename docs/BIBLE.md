# CASTALIA DOS — Technical Bible (Master Index)

This is the top of the CASTALIA DOS technical bible: a map of every section,
plus the final implementation recommendation, the first-weekend build plan, and
the first ten commands to run on a modern development machine.

CASTALIA DOS is a practical, beautiful, game-focused DOS-compatible operating
environment for real 386SX/386DX/486/early-Pentium hardware, built on FreeDOS
with an original suite of Castalia tools, profiles, launcher, installer, and
documentation. Compatibility beats elegance; the core stays faithfully DOS-like
while everything around it gets dramatically better.

## Section index

| § | Title | Where |
|---|---|---|
| 1 | Executive Vision | [`VISION.md`](VISION.md) |
| 2 | Legal and Licensing Strategy | [`LICENSE-STRATEGY.md`](LICENSE-STRATEGY.md) |
| 3 | Compatibility Goals | [`COMPATIBILITY.md`](COMPATIBILITY.md) |
| 4 | Technical Architecture | [`ARCHITECTURE.md`](ARCHITECTURE.md) |
| 5 | Boot Process Design | [`BOOT.md`](BOOT.md) |
| 6 | Memory Management Strategy | [`MEMORY.md`](MEMORY.md) |
| 7 | Game Launcher Design | [`LAUNCHER.md`](LAUNCHER.md) + [`../src/launch/LAUNCH.C`](../src/launch/LAUNCH.C) |
| 8 | Castalia Main Menu | [`LAUNCHER.md`](LAUNCHER.md) + [`../src/castalia/CASTALIA.C`](../src/castalia/CASTALIA.C) |
| 9 | Sound Configuration | [`SOUND.md`](SOUND.md) |
| 10 | Hardware Diagnostics | [`DIAGNOSTICS.md`](DIAGNOSTICS.md) |
| 11 | Installer Design | [`INSTALL.md`](INSTALL.md) |
| 12 | File Manager | [`FILE-MANAGER.md`](FILE-MANAGER.md) |
| 13 | Documentation System | [`HELP.md`](HELP.md) |
| 14 | Developer Toolchain | [`DEVELOPER.md`](DEVELOPER.md) |
| 15 | Repository Structure | this file + [`../README.md`](../README.md) |
| 16 | Build and Distribution Plan | [`DISTRIBUTION.md`](DISTRIBUTION.md) |
| 17 | Initial Working Files | [`../config/`](../config/), [`../src/`](../src/), [`../Makefile`](../Makefile), [`../scripts/build-floppy.md`](../scripts/build-floppy.md) |
| 18 | CONFIG.SYS / AUTOEXEC.BAT final draft | [`../config/CONFIG.SYS`](../config/CONFIG.SYS), [`../config/AUTOEXEC.BAT`](../config/AUTOEXEC.BAT) |
| 19 | 386SX Performance Budget | [`PERFORMANCE.md`](PERFORMANCE.md) |
| 20 | Compatibility Test Matrix | [`TESTING.md`](TESTING.md) |
| 21 | Roadmap | [`ROADMAP.md`](ROADMAP.md) |
| 22 | Risk Register | [`RISK-REGISTER.md`](RISK-REGISTER.md) |
| 23 | Product Personality and Branding | [`BRANDING.md`](BRANDING.md) |
| 24 | Future Graphical Shell | [`FUTURE-SHELL.md`](FUTURE-SHELL.md) |
| 25 | Final Recommendation | this file (below) |

## 15. Repository structure (as built)

```
castalia-dos/
├── README.md              project front door + repo map
├── LICENSE                MIT (original Castalia code)
├── Makefile               Open Watcom wmake build for the tools
├── LICENSES/              license texts + manifest (README.md)
├── docs/                  the technical bible (this directory)
├── config/                CONFIG.SYS, AUTOEXEC.BAT, *.INI, *.BAT
├── src/
│   ├── common/            ini.c/.h, ui.c/.h  (shared libraries)
│   ├── launch/            LAUNCH.C  (game launcher)
│   ├── castalia/          CASTALIA.C (main menu)
│   └── <tool>/            one directory per tool (see APPS.md)
├── build/                 build output (.obj/.exe)
├── dist/                  release images (floppy/ZIP/ISO/CF)
├── floppy/                floppy-image staging (payload/, boot sector)
├── help/                  plain-text help pages + HELP.IDX
├── scripts/               build-floppy.md (+ future .sh)
├── third_party/           notes on the FreeDOS components (fetched at build time)
├── tools/                 host-side helpers, emulator configs
└── tests/                 test assets and logs
```

Each folder's role is explained in [`../README.md`](../README.md) and
[`ARCHITECTURE.md`](ARCHITECTURE.md).

---

# 25. Final Recommendation

## What to build first

**Build the bootable core, not the tools.** The single highest-value artifact
is a floppy that boots a real 386SX into the Castalia boot menu, lets the user
pick a memory profile, and lands at a working prompt with the right amount of
free conventional memory. That artifact proves the whole thesis — FreeDOS core
+ Castalia profiles — and everything else (launcher, menu, sound, installer)
hangs off it. The C tools are already prototyped in this repo; they are the
*second* thing, not the first.

Concretely, the build order is:

1. **Bootable Castalia floppy** — FreeDOS kernel/shell + `config/CONFIG.SYS`
   menu + `config/AUTOEXEC.BAT` branching. (Proves boot + profiles.)
2. **Memory profiles verified** — `MEM /C` on each profile matches the targets
   in [`MEMORY.md`](MEMORY.md). (Proves the memory thesis.)
3. **`CASTALIA.EXE` + `LAUNCH.EXE`** — the menu and launcher from `src/`, with
   `GAMES.INI`. (Proves the experience.)
4. **Sound + CD + mouse config** — `SETSOUND.EXE`, the CDROM profile.
5. **`SETUP.EXE`** — install to hard disk / CF.

That order matches [`ROADMAP.md`](ROADMAP.md) phases 1→5 and always keeps a
bootable, demonstrable system in hand.

## First weekend build

A realistic two-day plan that ends with a bootable, profile-switching CASTALIA
DOS floppy tested in an emulator and ready for a real 386SX.

### Day 1 — Boot and profiles

- Create the repo and folder layout (already done here).
- Collect the legal FreeDOS components into `floppy/payload/` (kernel, shell,
  `HIMEMX`, `JEMM386`, base utilities) and the FreeDOS floppy boot sector.
- Build the bootable floppy image following
  [`../scripts/build-floppy.md`](../scripts/build-floppy.md).
- Drop in the Castalia boot screen / banner (from
  [`BRANDING.md`](BRANDING.md)).
- Install `config/CONFIG.SYS` (the 8-item menu, in the FreeDOS kernel's own
  `MENU` + `n?` syntax — see [`BOOT.md`](BOOT.md)) and `config/AUTOEXEC.BAT`
  (which turns the `%CONFIG%` digit into `%CASTPROFILE%` and branches on it).
- Boot it in DOSBox-X; confirm the menu appears and each profile boots.

### Day 2 — Tools and test

- Add the memory profiles' verification: run `MEM /C` per profile, record the
  free conventional memory, compare to [`MEMORY.md`](MEMORY.md).
- Build `CASTALIA.EXE` and `LAUNCH.EXE` with `wmake`; copy them and
  `config/GAMES.INI` onto the image.
- Wire the menu loop and batch handoff (`GAMES.BAT`), and confirm "Launch
  Games" opens the launcher and that a selected game's `_RUNGAME.BAT` is
  generated.
- Add a sample `GAMES.INI` with a couple of freely-distributable titles.
- Test the full path in DOSBox-X, then in 86Box configured as a 386SX for
  accurate timing.
- Prepare the image for a real 386SX test (write to a CF card or a physical
  floppy).

At the end of the weekend you have CASTALIA DOS 0.1 "Almenara": a bootable,
profile-switching, menu-driven DOS floppy — the spine of the whole product.

## The first 10 commands on a modern dev machine

Assuming a Linux host (the recipe in `scripts/build-floppy.md`), with
`mtools`, `dd`, and Open Watcom V2 installed, and the FreeDOS payload gathered
in `floppy/payload/`:

```sh
# 1. Get the project.
git clone <your-castalia-dos-remote> && cd castalia-dos

# 2. Load the Open Watcom cross-build environment.
. /opt/watcom/owsetenv.sh          # sets WATCOM, PATH, INCLUDE

# 3. Build the Castalia tools (16-bit real-mode DOS EXEs).
wmake

# 4. Create a blank 1.44 MB floppy image.
dd if=/dev/zero of=dist/castalia-boot.img bs=512 count=2880

# 5. Format it FAT12 with a FreeDOS boot sector.
mformat -i dist/castalia-boot.img -f 1440 -B floppy/fdboot.bin ::

# 6. Make the directory layout.
mmd -i dist/castalia-boot.img ::/DOS ::/CASTALIA ::/CASTALIA/BIN ::/CASTALIA/CFG

# 7. Copy the kernel, shell, and Castalia config to the root/tree.
mcopy -i dist/castalia-boot.img floppy/payload/KERNEL.SYS floppy/payload/COMMAND.COM config/CONFIG.SYS config/AUTOEXEC.BAT ::/

# 8. Copy the memory managers and Castalia tools + config.
mcopy -i dist/castalia-boot.img floppy/payload/HIMEMX.EXE floppy/payload/JEMM386.EXE ::/DOS/ && mcopy -i dist/castalia-boot.img build/castalia.exe build/launch.exe config/GAMES.BAT ::/CASTALIA/BIN/ && mcopy -i dist/castalia-boot.img config/*.INI config/SOUND.BAT ::/CASTALIA/CFG/

# 9. Verify the image contents.
mdir -i dist/castalia-boot.img ::/ && mdir -i dist/castalia-boot.img ::/CASTALIA/BIN

# 10. Boot it in an emulator.
dosbox-x dist/castalia-boot.img         # then, for accuracy, boot it in 86Box as a 386SX
```

From here, iterate: verify the profiles with `MEM /C`, flesh out `SETSOUND`,
`HWINFO`, and `SETUP`, grow `GAMES.INI`, and validate on real hardware. The
roadmap in [`ROADMAP.md`](ROADMAP.md) carries it from 0.1 "Almenara" to 1.0
"Tombatossals".

## The one-sentence version

Ship a FreeDOS-based, profile-driven, beautifully documented DOS gaming
environment that boots real 386SX hardware; build the bootable core first, keep
every tool lean and text-mode, guard the memory budget above all, and grow the
original Castalia layer one honest component at a time.
