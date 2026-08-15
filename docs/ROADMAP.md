# 21. Roadmap

**CASTALIA DOS — Technical Bible, Section 21**
**Flagship target: CASTALIA DOS 386SX Edition**
Document license: CC BY 4.0 · © 2026 The Castalia DOS Project (repo owner
`davabe`).

This section defines the phased delivery plan for Castalia DOS from an empty
repository to a stable 1.0 release, plus one exploratory phase beyond. It is
written for the realistic case: a **solo maintainer or a very small team**
(one to three people, evenings and weekends), building on **FreeDOS** and other
open/free DOS components, with all original Castalia code under **MIT** and all
modified GPL components kept in `third_party/` with source.

The plan front-loads two things that are cheap to get wrong and expensive to
fix later: **legal cleanliness** and **bootability on real hardware**. Features,
menus, and polish come only after a machine actually boots Castalia unattended.

---

## 21.1 Sequencing rationale

The order of phases is not arbitrary. Five constraints drive it.

1. **Legal foundation before any code ships (Phase 0 first).** Castalia's whole
   value proposition is "a serious, *legally-clean* DOS." A stray Microsoft
   binary or a GPL/MIT linking mistake makes the project worthless no matter how
   good the menus are, and it compounds with every later commit. The license
   audit, `LICENSES/` tree, and `third_party/` separation rules exist **before**
   the first component is vendored.

2. **Bootability before features (Phase 1 before 2+).** A DOS that does not boot
   is not a DOS. Profiles, menus, sound, and the installer all assume a machine
   that reaches `C:\>` from cold. We prove that with the smallest possible
   FreeDOS system first, in an emulator, before writing any Castalia C code.

3. **Profiles before the installer (Phase 2 before 5).** The installer's main
   job is to *write correct `CONFIG.SYS`/`AUTOEXEC.BAT` for the eight profiles*
   onto a disk. You cannot automate emission of files you have not yet designed
   and hand-verified, so profiles are authored and tested by hand first; the
   installer later just reproduces that known-good output.

4. **Hardware validation before the public alpha (Phase 7 before 8).** Emulators
   are necessary but not sufficient — real 386SX/486 timing, Sound Blaster
   IRQ/DMA, IDE/CompactFlash quirks, and UMB layout differ from emulation. An
   alpha that only ran under emulation invites a flood of "won't boot on my
   machine" reports we cannot triage. Validating on metal first keeps alpha
   feedback about *features*, not *basic survival*.

5. **Original kernel exploration last, and clearly optional (Phase 10).**
   Writing an original DOS-compatible kernel is a multi-year, high-failure-rate
   research effort that must never block a shippable product. FreeDOS carries
   1.0; the kernel track only starts after 1.0 is stable, and only if there is a
   concrete reason FreeDOS cannot serve.

The through-line: **compatibility beats elegance** (per project conventions),
so every phase is gated on objective, testable behaviour — "it boots," "it
frees N KB," "it runs game X" — not on subjective polish.

---

## 21.2 Codename mapping

Version codenames are Valencian/Castellón fortress towns. Phases map to the
first release that *contains* their deliverables; several phases can land inside
one release.

| Release | Codename       | Phases delivered            | Theme                                   |
|---------|----------------|-----------------------------|-----------------------------------------|
| (pre)   | —              | Phase 0                     | Legal + research foundation             |
| 0.1     | Almenara       | Phase 1                     | It boots. Minimal FreeDOS Castalia      |
| 0.2     | Peñíscola      | Phases 2, 3, 4              | Profiles, menu, launcher, sound/CD/mouse|
| 0.5     | Morella        | Phases 5, 6                 | Installer + game compatibility database |
| 1.0-rc  | Tombatossals\* | Phases 7, 8                 | Hardware validation + public alpha      |
| 1.0     | Tombatossals   | Phase 9                     | Stable release                          |
| 1.1     | Montornés      | Phase 10 (+ `CASTFM.EXE`)   | File manager + original-kernel research |

\* The 0.9x/alpha builds on the road to 1.0 carry the Tombatossals codename with
a pre-release suffix (e.g. `1.0-alpha1`), since they are the release candidates
for that version. Almenara is named for the 386SX-era simplicity of the first
bootable image; Tombatossals (the giant of Castellón folklore) is deliberately
reserved for the "big, stable, complete" 1.0.

---

## 21.3 Complexity scale

Estimates are **relative effort for a solo maintainer**, not calendar dates.
Retro/DOS work is slow: a 16-bit real-mode bug can eat a weekend, and every
change wants testing across emulator + real hardware.

| Size | Meaning                        | Rough solo effort  | Typical content                          |
|------|--------------------------------|--------------------|------------------------------------------|
| S    | Small, well-scoped             | days – 1–2 weeks   | one tool, one config file, one script    |
| M    | Medium, several moving parts   | 2–6 weeks          | a subsystem + its tests + docs           |
| L    | Large, integration-heavy       | 1.5–3 months       | multi-tool feature, installer, DB tooling|
| XL   | Very large / open-ended        | 3+ months to years | hardware campaign, original kernel       |

---

## 21.4 Dependency graph (at a glance)

```
Phase 0  Legal/Research
   |
   v
Phase 1  Bootable prototype (0.1 Almenara)
   |
   +-------------------+-------------------+
   v                   v                   v
Phase 2  Profiles   (needs 1)
   |
   v
Phase 3  Menu+Launcher (needs 1,2)
   |
   v
Phase 4  Sound/CD/Mouse (needs 1,2,3)
   |
   +--------> Phase 6  Game DB (needs 3,4)
   |               |
   v               |
Phase 5  Installer (needs 2,3,4) <---- reuses 6 data when present
   |               |
   +-------+-------+
           v
Phase 7  Hardware validation (needs 1-6)
           |
           v
Phase 8  Public alpha (needs 7)
           |
           v
Phase 9  1.0 release (needs 8)
           |
           v
Phase 10 Original kernel research (needs 9; optional, non-blocking)
```

---

## 21.5 Phase 0 — Research and legal foundation

**Codename target:** pre-0.1 (foundation for Almenara)

### Goals
- Establish an airtight legal and licensing basis so that no later phase can
  accidentally introduce Microsoft-owned code, text, or branding.
- Choose and pin the exact open/free components (versions, upstream URLs,
  licenses) that Castalia will vendor into `third_party/`.
- Stand up the toolchain and repository skeleton so Phase 1 can start building
  immediately.

### Deliverables (concrete files/artifacts)
| Artifact                              | Purpose                                            |
|---------------------------------------|----------------------------------------------------|
| `LICENSE`                             | MIT for original Castalia code (already present).   |
| `LICENSES/GPL-2.0.txt`, `BSD-2.txt`…  | Full text of every license used by vendored comps. |
| `docs/LEGAL.md`                       | Provenance rules; MIT↔GPL separation policy.        |
| `third_party/MANIFEST.md`             | Table: component, version, upstream, license, SHA.  |
| `third_party/` layout stubs           | One dir per component with its own `LICENSE`+source.|
| `docs/COMPONENTS.md`                  | The pinned open/free stack (see below).             |
| `build/README` + Open Watcom setup    | Documented `wcc`/`wcl`/`wmake` toolchain install.   |
| `tools/verify-licenses.sh`            | Script asserting every binary traces to a license.  |
| `.gitattributes` for CRLF/text        | DOS line-ending handling (already seeded).          |

Pinned component stack recorded in `docs/COMPONENTS.md`:

| Role            | Component                | License   | Notes                          |
|-----------------|--------------------------|-----------|--------------------------------|
| Kernel          | FreeDOS `KERNEL.SYS`     | GPLv2+    | reports 7.x; see SETVER policy |
| Shell           | FreeDOS `COMMAND.COM`    | GPLv2+    | FreeCOM                        |
| XMS manager     | `HIMEMX.EXE`             | open/free | XMS provider                   |
| EMM / UMB       | `JEMM386.EXE`            | open/free | EMM386-compatible; FreeDOS     |
|                 |                          |           | `EMM386` as fallback           |
| Mouse           | `CTMOUSE`                | GPL       | serial + PS/2; ship source     |
| CD-ROM driver   | `UIDE.SYS`               | open/free | IDE/ATAPI                      |
| MSCDEX replace  | `SHSUCDX`                | open/free | loaded high                    |
| Keyboard        | FreeDOS `KEYB`           | GPLv2+    | national layouts               |
| Disk cache      | `SMARTDRV`-style cache   | LGPL/free | write-through default          |
| Version report  | `SETVER`                 | GPLv2+    | per-program spoof only         |

### Risks
- **Provenance drift:** a "convenient" binary of unknown origin sneaks in.
  *Mitigation:* every `third_party/` component must ship with source + upstream
  URL + license, enforced by `tools/verify-licenses.sh` in CI.
- **MIT/GPL contamination:** static-linking GPL code into MIT tools would force
  relicensing. *Mitigation:* architectural rule — Castalia tools are *separate
  executables/drivers* that talk to FreeDOS components; **never** static-link
  GPL objects into MIT binaries. Documented in `docs/LEGAL.md`.
- **Version-spoofing overreach:** claiming a global "MS-DOS 6.22" spoof is both
  false and a compatibility trap. *Mitigation:* policy fixed now — kernel
  reports FreeDOS 7.x; Castalia presents *6.22-compatible behaviour*; `SETVER`
  is used per-program only.
- **Trademark:** "Castalia DOS" naming and art must be original. *Mitigation:*
  branding marks are proprietary-original; permissive art only.

### Exit criteria (objective, testable)
- `tools/verify-licenses.sh` passes: every file under `third_party/` maps to a
  known license and has recorded upstream + checksum.
- `docs/LEGAL.md` and `third_party/MANIFEST.md` reviewed and committed; zero
  Microsoft-owned artifacts present (grep audit of binaries clean).
- Open Watcom builds a trivial `hello.exe` for the DOS target via `wmake`,
  proving the toolchain end-to-end (`wcl -0 -bt=dos -ml -os`).
- A written one-page decision: FreeDOS as the 1.0 foundation, original kernel
  deferred to Phase 10.

### Estimated complexity: **M**
Little code, but the legal review, component vetting, and toolchain setup are
detail-heavy and unforgiving. Getting this right is what makes the whole project
defensible; getting it wrong poisons every later phase.

### Dependencies
None. This is the root of the graph.

---

## 21.6 Phase 1 — Bootable FreeDOS-based Castalia prototype

**Codename target:** 0.1 "Almenara"

### Goals
- Produce the smallest possible bootable Castalia image that reaches `C:\>` from
  a cold start, first from floppy, then from an IDE/CompactFlash image.
- Prove the boot chain, the FAT12/FAT16 disk layout, and the emulator test loop
  (DOSBox-X + 86Box/PCem) end-to-end.
- Establish the reproducible build that assembles a disk image from
  `third_party/` components + a minimal Castalia branding layer.

### Deliverables (concrete files/artifacts)
| Artifact                          | Purpose                                             |
|-----------------------------------|-----------------------------------------------------|
| `floppy/CASTALIA.IMG`             | Bootable 1.44 MB floppy image (build output).       |
| `dist/castalia-386sx.img`         | Bootable hard-disk/CF image (FAT16).                |
| `build/mkimage.sh` / `Makefile`   | Reproducible image assembly script.                 |
| `config/CONFIG.SYS` (minimal)     | Single-config boot: `HIMEMX` + `DOS=HIGH`.          |
| `config/AUTOEXEC.BAT` (minimal)   | PATH, prompt, Castalia banner.                      |
| `src/branding/BANNER.*`           | Calm 80x25 blue/gray/amber boot banner.             |
| `C:\DOS` population step          | FreeDOS base utils + memory managers placed.        |
| `docs/BUILD.md`                   | How to build and run the image in each emulator.    |
| `tests/boot-smoke.md`             | Manual smoke checklist; later automated.            |

Example first-boot banner (within 80 columns):

```
  ####################################################################
  #  CASTALIA DOS  -  386SX Edition            0.1 "Almenara"        #
  #  DOS-compatible environment  -  FreeDOS foundation              #
  #  (c) 2026 The Castalia DOS Project                              #
  ####################################################################
  Memory: conventional OK   XMS: present   DOS in HMA
  Type CASTALIA later; for now you have a working command prompt.

  C:\>
```

### Risks
- **Boot sector / kernel load fails on odd geometry** (CF cards report unusual
  CHS). *Mitigation:* test multiple geometries in 86Box; document a known-good
  CF partitioning recipe.
- **File case / line endings** corrupt DOS files when built on Linux.
  *Mitigation:* `.gitattributes` + build step forces CRLF and 8.3 names.
- **Emulator-only success masking real-mode issues.** *Mitigation:* keep the
  image dead-simple so Phase 7 hardware bring-up has minimal surface.

### Exit criteria (objective, testable)
- `dist/castalia-386sx.img` cold-boots to `C:\>` in DOSBox-X **and** 86Box
  (emulated 386SX, 4 MB RAM) unattended, no manual key presses.
- `MEM` shows `DOS=HIGH` in the HMA and XMS present via `HIMEMX`.
- The image is produced solely by `build/mkimage.sh` from committed sources +
  `third_party/` — no hand-edited disk.
- `chkdsk`/`fdisk /status` equivalent shows a clean FAT16 volume.

### Estimated complexity: **M**
Assembling a bootable FreeDOS image is well-trodden, but the reproducible build,
image geometry, and dual-emulator loop take real iteration. No original C tools
yet, which keeps it out of L.

### Dependencies
Phase 0 (pinned components, toolchain, legal layout).

---

## 21.7 Phase 2 — Memory profiles and boot menu

**Codename target:** 0.2 "Peñíscola" (first of three)

### Goals
- Author the canonical eight-entry boot menu and its layered memory profiles by
  hand, and verify each one's free-conventional-RAM and driver posture.
- Establish the `CONFIG.SYS` `%CONFIG%` menu structure that every later phase
  and the installer will reproduce.

### Deliverables (concrete files/artifacts)
| Artifact                        | Purpose                                              |
|---------------------------------|------------------------------------------------------|
| `config/CONFIG.SYS`             | Full 8-entry `[menu]` + per-block `%CONFIG%` logic.  |
| `config/AUTOEXEC.BAT`           | Branches on `%CONFIG%` to load the right TSRs.       |
| `C:\CASTALIA\CFG\PROFILES.INI`  | Machine-readable description of each profile.         |
| `docs/PROFILES.md`              | Rationale + measured free RAM per profile.            |
| `tests/mem-matrix.md`           | Per-profile `MEM` expectations (pass/fail table).     |
| `C:\CASTALIA\BACKUP\` seeding   | Location for user config backups (used by installer). |

Canonical menu (order and labels are fixed by project conventions):

```
        CASTALIA DOS - Boot Menu            0.2 "Pen...iscola"
   ---------------------------------------------------------------
    1. CLEAN   Maximum Compatibility (most free RAM)
    2. XMS     XMS Gaming            (default)
    3. EMS     EMS Gaming
    4. CDROM   CD-ROM Gaming
    5. WIN3X   Windows 3.x Mode
    6. DIAG    Diagnostics
    7. SAFE    Safe Mode
    8. PROMPT  Command Prompt Only
   ---------------------------------------------------------------
    Time: 10s   Default: [2] XMS
```

Profile layering (canonical design; must be reproduced exactly):

| Profile | Base                                   | Adds                          | Target free conv. |
|---------|----------------------------------------|-------------------------------|-------------------|
| SAFE    | `KERNEL.SYS`+`COMMAND.COM`, DOS low     | nothing (rescue)              | n/a (rescue)      |
| CLEAN   | `HIMEMX` + `DOS=HIGH`                    | no EMM386/UMB/TSR             | ~615 KB           |
| XMS     | `HIMEMX`+`JEMM386 NOEMS`+`DOS=HIGH,UMB`  | mouse+sound env high, no EMS  | ~620–631 KB       |
| EMS     | `HIMEMX`+`JEMM386 FRAME=E000`+`,UMB`     | EMS page frame + UMB          | lower (frame cost)|
| CDROM   | EMS base                                | `UIDE.SYS`+`SHSUCDX` high     | lower still       |
| WIN3X   | `HIMEMX`+`JEMM386`(EMS)+`,UMB`           | mouse + `SMARTDRV`            | tuned for Win3.1  |
| DIAG    | CLEAN base                              | AUTOEXEC runs `HWINFO.EXE`    | ~= CLEAN          |
| PROMPT  | CLEAN base                              | drops to bare prompt, no menu | ~= CLEAN          |

### Risks
- **UMB layout collides with adapters** (video/network ROM at C000–EFFF).
  *Mitigation:* conservative `JEMM386` include/exclude defaults; document
  `X=` exclusions; validated for real on Phase 7.
- **EMS page frame placement** conflicts on some boards. *Mitigation:* default
  `FRAME=E000`, with a documented fallback frame.
- **Free-RAM targets not met** because a TSR loads low. *Mitigation:* the
  `tests/mem-matrix.md` gate makes this measurable, not vibes.

### Exit criteria (objective, testable)
- All 8 menu entries boot without hanging in DOSBox-X and 86Box.
- `MEM /C` output per profile matches `tests/mem-matrix.md` within tolerance:
  XMS ≥ 620 KB free conventional; CLEAN ≥ 615 KB; EMS shows a valid page frame.
- SAFE boots with zero drivers and reaches a prompt (rescue proven).
- PROMPT bypasses the Castalia menu and lands at bare `C:\>`.

### Estimated complexity: **L**
The profiles are the technical heart of the product and each interacts with
memory managers, UMBs, and driver load order. Getting eight of them correct and
*measured* is genuinely hard and iteration-heavy on emulators.

### Dependencies
Phase 1 (bootable base, build system). Phase 0 (component set: `JEMM386`,
`HIMEMX`, `UIDE`, `SHSUCDX`, `SMARTDRV`, `CTMOUSE`).

---

## 21.8 Phase 3 — Castalia menu and launcher

**Codename target:** 0.2 "Peñíscola"

### Goals
- Ship the first original Castalia C tools: the main text-mode menu
  (`CASTALIA.EXE`) and the game launcher (`LAUNCH.EXE`).
- Establish the shared 80x25, 16-color VGA text-UI toolkit (panels, frames,
  selection bar) used by all later tools, in the Castalia palette.

### Deliverables (concrete files/artifacts)
| Artifact                          | Purpose                                              |
|-----------------------------------|------------------------------------------------------|
| `src/castalia/CASTALIA.EXE`       | Main text-mode menu / shell front-end.               |
| `src/launch/LAUNCH.EXE`           | Game launcher (alt name `GAMEVAULT.EXE`).            |
| `src/ui/` (shared)                | Reusable text-UI: box draw, colors, keyboard, menus. |
| `C:\CASTALIA\CFG\CASTALIA.INI`    | Menu contents, theme, default actions.               |
| `C:\CASTALIA\CFG\GAMES.INI`       | Launcher's game list (title, path, exe, profile).    |
| `C:\CASTALIA\HELP\*.HLP`          | Help text pages surfaced by the menu.                |
| `docs/UI-STYLE.md`                | Palette + attribute rules (blue bg, amber highlight).|
| `tests/ui-nav.md`                 | Keyboard-navigation checklist.                       |

Example main menu (80-column, 16-color; attributes described in `UI-STYLE.md`):

```
  +==============================================================+
  |  CASTALIA DOS                          0.2 "Pen...iscola"    |
  +==============================================================+
  |                                                              |
  |     > Games                                                  |
  |       Memory Profile                                         |
  |       Sound Setup                                            |
  |       Diagnostics                                            |
  |       Help                                                   |
  |       Command Prompt                                         |
  |                                                              |
  +--------------------------------------------------------------+
  |  Up/Down move   Enter select   F1 Help   Esc Prompt          |
  +==============================================================+
```

### Risks
- **Toolchain portability:** code must build under Open Watcom *and* be Turbo C
  friendly. *Mitigation:* C89 only, fixed buffers, no dynamic allocation, small
  functions; CI builds with `wcl -0 -bt=dos -ml -os`.
- **UI too heavy for 386SX/limited RAM.** *Mitigation:* direct B800 text writes,
  no bloated runtime, measure the `.EXE` footprint as a gate.
- **Launcher shells out and loses memory** to the child game. *Mitigation:*
  launcher can exit-to-run (swap itself out) so the game gets full conventional
  RAM, matching the "compatibility beats elegance" rule.

### Exit criteria (objective, testable)
- `CASTALIA.EXE` launches from `AUTOEXEC.BAT`, renders the menu in the correct
  palette, and is fully keyboard-navigable (arrows/Enter/Esc/F1).
- `LAUNCH.EXE` reads `GAMES.INI`, lists entries, and launches at least one
  bundled/test DOS program, returning cleanly to the menu afterward.
- Both binaries build reproducibly under Open Watcom via `wmake` with zero
  warnings at the project's flag set; a Turbo C spot-build also compiles.
- Selecting "Command Prompt" exits to `C:\>`; re-running `CASTALIA` returns.

### Estimated complexity: **M**
First real C tools plus a reusable UI layer. Bounded scope (two tools + toolkit)
keeps it at M, but the shared UI decisions here shape every later tool, so it
must be done carefully.

### Dependencies
Phase 1 (boot + build), Phase 2 (profiles exist so the menu can reference the
active `%CONFIG%` and offer a profile switch stub).

---

## 21.9 Phase 4 — Sound / CD-ROM / mouse configuration

**Codename target:** 0.2 "Peñíscola" (completes the release)

### Goals
- Make hardware setup approachable: original tools to configure sound
  (`SETSOUND.EXE`), and wire CD-ROM (`UIDE.SYS`+`SHSUCDX`) and mouse
  (`CTMOUSE`) into the profiles and menu.
- Emit correct `SET BLASTER=` strings and driver load lines that the profiles
  and (later) the installer consume.

### Deliverables (concrete files/artifacts)
| Artifact                         | Purpose                                               |
|----------------------------------|-------------------------------------------------------|
| `src/setsound/SETSOUND.EXE`      | Interactive sound-card config; writes env + INI.       |
| `C:\CASTALIA\CFG\` sound section | Persisted `SET BLASTER=` + card profile.               |
| `C:\CASTALIA\DRV\`               | Bundled `CTMOUSE`, `UIDE.SYS`, `SHSUCDX` (from 3P).    |
| CDROM profile wiring             | `UIDE.SYS` in `CONFIG.SYS`, `SHSUCDX` high in AUTOEXEC.|
| Mouse wiring                     | `CTMOUSE` load in XMS/EMS/CDROM/WIN3X profiles.        |
| `docs/SOUND.md`                  | `SET BLASTER` fields + supported card profiles.        |
| `tests/audio-cd-mouse.md`        | Detection/verification checklist.                      |

`SET BLASTER` field reference (per project conventions):

```
  SET BLASTER=A220 I5 D1 H5 T4
              |    |  |  |  |
              |    |  |  |  +-- T: card type code (e.g. SB16=6, SBPro=4)
              |    |  |  +----- H: 16-bit DMA channel
              |    |  +-------- D: 8-bit DMA channel
              |    +----------- I: IRQ
              +---------------- A: I/O base port
```

Supported sound profiles: None · PC Speaker · AdLib · SB 1.5 · SB 2.0 ·
SB Pro · SB 16. (Roland MT-32 / General MIDI deferred to a later minor.)

### Risks
- **No safe autodetect on ISA** (probing wrong ports can hang). *Mitigation:*
  `SETSOUND.EXE` defaults to *guided manual* selection; any probe is opt-in and
  conservative; known-good presets per card.
- **IRQ/DMA conflicts** between sound, CD, and mouse. *Mitigation:* document a
  conflict matrix; validate combos in Phase 7 on real hardware.
- **SHSUCDX/UIDE load order** wrong → no CD drive letter. *Mitigation:* fixed,
  tested order captured in the CDROM profile and `tests/audio-cd-mouse.md`.

### Exit criteria (objective, testable)
- `SETSOUND.EXE` writes a valid `SET BLASTER=` line; selecting SB Pro and
  booting XMS exposes the env var to a test that reads it (e.g. a small DOS
  program or a game's setup detecting the card in DOSBox-X).
- CDROM profile assigns a CD drive letter via `SHSUCDX` and can `DIR` an
  emulated ISO/CD.
- Mouse works (cursor moves, buttons register) under `CTMOUSE` in XMS profile.
- All three configured simultaneously in the CDROM profile without a hang and
  still meeting that profile's minimum free-RAM target.

### Estimated complexity: **M**
One new tool plus integration of three drivers into existing profiles. The risk
is in hardware quirks (deferred to Phase 7), so the emulator-side work is
moderate.

### Dependencies
Phase 2 (profiles to wire into), Phase 3 (menu + UI toolkit for `SETSOUND`).

---

## 21.10 Phase 5 — Installer

**Codename target:** 0.5 "Morella" (first of two)

### Goals
- Deliver `SETUP`/installer that partitions/formats a target (or installs to an
  existing FAT16 volume), lays down the directory structure, copies FreeDOS +
  Castalia files, and **writes the known-good `CONFIG.SYS`/`AUTOEXEC.BAT`** for
  all eight profiles.
- Make Castalia installable unattended onto real CF/IDE media, with backups of
  any pre-existing user config.

### Deliverables (concrete files/artifacts)
| Artifact                          | Purpose                                             |
|-----------------------------------|-----------------------------------------------------|
| `src/setup/SETUP.EXE`             | Text-mode installer.                                 |
| `scripts/` install helpers        | fdisk/format orchestration, file copy manifests.     |
| Directory-layout emitter          | Creates `C:\DOS`, `C:\CASTALIA\{BIN,DRV,CFG,HELP,`   |
|                                   | `GAMES,TOOLS,BACKUP}`, `C:\GAMES`.                    |
| Config emitter                    | Writes the Phase 2 `CONFIG.SYS`/`AUTOEXEC.BAT`.      |
| `C:\CASTALIA\BACKUP\` on install  | Saves user's prior `CONFIG.SYS`/`AUTOEXEC.BAT`.     |
| `dist/castalia-setup-floppy.img`  | Boot+install floppy set.                             |
| `dist/castalia-install-cd.iso`    | Optional install ISO for CD-equipped machines.       |
| `docs/INSTALL.md`                 | Step-by-step + recovery instructions.                |
| `tests/install-matrix.md`         | Fresh-disk, existing-disk, upgrade scenarios.        |

Installed layout the installer must produce (per project conventions):

```
  C:\ KERNEL.SYS  COMMAND.COM  CONFIG.SYS  AUTOEXEC.BAT
  C:\DOS\              FreeDOS base utils + memory managers
  C:\CASTALIA\BIN\     Castalia tools (on PATH)
  C:\CASTALIA\DRV\     Castalia-bundled drivers
  C:\CASTALIA\CFG\     CASTALIA.INI  PROFILES.INI  GAMES.INI
  C:\CASTALIA\HELP\    help text pages
  C:\CASTALIA\GAMES\   games      C:\GAMES\   games
  C:\CASTALIA\TOOLS\   extra tools
  C:\CASTALIA\BACKUP\  backups of user CONFIG.SYS/AUTOEXEC.BAT
```

### Risks
- **Destructive fdisk/format on the wrong drive.** *Mitigation:* explicit target
  confirmation, refuse to touch the boot medium, dry-run mode, mandatory backup
  step before overwriting any existing config.
- **Emitted config drifts from hand-tested profiles.** *Mitigation:* the
  installer emits from the *same* templates verified in Phase 2; a test compares
  installer output byte-for-byte against the golden `config/`.
- **Low-RAM install medium** (installer itself must run in conventional RAM).
  *Mitigation:* installer built to the same lean constraints as other tools.

### Exit criteria (objective, testable)
- Unattended install to a blank FAT16 CF image in 86Box yields a disk that
  cold-boots straight into the Castalia menu with all 8 profiles working.
- Installing over an existing DOS install backs up the prior
  `CONFIG.SYS`/`AUTOEXEC.BAT` into `C:\CASTALIA\BACKUP\` before writing.
- `tests/install-matrix.md` passes for: fresh disk, existing FAT16 volume, and
  re-install/upgrade.
- Installer-emitted `CONFIG.SYS`/`AUTOEXEC.BAT` match the Phase 2 golden files.

### Estimated complexity: **L**
Installers are integration-heavy and dangerous (they format disks). Partitioning
across geometries, safe config emission, and multi-scenario testing make this a
solid L.

### Dependencies
Phase 2 (golden config to emit), Phase 3 (UI toolkit + menu), Phase 4 (driver
files to install). Uses Phase 6 data if available, but does not block on it.

---

## 21.11 Phase 6 — Game compatibility database

**Codename target:** 0.5 "Morella"

### Goals
- Build the data + tooling that maps games to a recommended boot profile, sound
  settings, and any per-game notes, surfaced through the launcher and
  `GAMECFG.EXE`.
- Turn Castalia's "game-focused" promise into structured, testable data.

### Deliverables (concrete files/artifacts)
| Artifact                          | Purpose                                              |
|-----------------------------------|------------------------------------------------------|
| `src/gamecfg/GAMECFG.EXE`         | Per-game config tool (profile, sound, notes).        |
| `C:\CASTALIA\CFG\GAMES.INI`       | Extended schema: title, exe, profile, blaster, notes.|
| `data/gamedb/` (source)           | Human-edited compatibility records (build input).    |
| `tools/gamedb-build.*`            | Compiles `data/gamedb/` → shipped `GAMES.INI`.       |
| `docs/COMPAT.md`                  | Compatibility methodology + column definitions.      |
| `tests/compat-sample.md`          | Verified sample set (Doom, Wolf3D, an EMS game, a    |
|                                   | CD game) with expected profile/sound.                |

Compatibility record shape (per game):

| Field    | Example              | Meaning                                   |
|----------|----------------------|-------------------------------------------|
| title    | DOOM                 | display name                              |
| exe      | DOOM.EXE             | launch target                             |
| profile  | XMS                  | recommended `%CONFIG%` profile            |
| sound    | SBPro A220 I5 D1     | recommended card + `SET BLASTER` hint     |
| minconv  | 560K                 | minimum free conventional RAM needed      |
| ems/xms  | XMS                  | memory type used                          |
| notes    | needs mouse off      | quirks / setup guidance                   |

### Risks
- **Unbounded scope** (thousands of DOS games). *Mitigation:* ship a curated,
  verified seed set for 1.0; make the schema and build tool the deliverable, so
  the DB grows by community contribution after launch.
- **Stale/unverifiable entries.** *Mitigation:* each shipped record cites how it
  was tested (emulator/hardware); `tests/compat-sample.md` gates the seed set.
- **Licensing of game data/art.** *Mitigation:* store only original factual
  metadata (title, settings), never game binaries or copyrighted assets.

### Exit criteria (objective, testable)
- `tools/gamedb-build` compiles `data/gamedb/` into a valid `GAMES.INI` the
  launcher reads without error.
- `GAMECFG.EXE` can view/edit a game's profile + sound and persist it.
- The seed set (≥ the four sample titles across CLEAN/XMS/EMS/CDROM) launches
  under the recommended profile in DOSBox-X with the DB-specified sound applied.
- `docs/COMPAT.md` defines every field and the verification method.

### Estimated complexity: **L**
The tooling and schema are moderate, but curating and *verifying* a credible
compatibility seed set across multiple profiles is time-consuming and is what
makes the feature trustworthy.

### Dependencies
Phase 3 (launcher reads the DB), Phase 4 (sound settings the DB references).
Feeds Phase 5 (installer can ship the DB) but is developed in parallel.

---

## 21.12 Phase 7 — Real hardware validation

**Codename target:** on the road to 1.0 "Tombatossals" (pre-alpha hardening)

### Goals
- Prove Castalia on actual period hardware: 386SX first, then 386DX, 486, and
  early Pentium, across real VGA, real Sound Blaster, real IDE/CompactFlash, and
  real PS/2 + serial mice.
- Convert emulator assumptions (UMB layout, timing, IRQ/DMA) into verified,
  documented facts and fix what breaks.

### Deliverables (concrete files/artifacts)
| Artifact                          | Purpose                                              |
|-----------------------------------|------------------------------------------------------|
| `docs/HARDWARE.md`                | Tested-machines matrix + known-good/known-bad notes. |
| `src/hwinfo/HWINFO.EXE`           | Diagnostics tool (DIAG profile); reports CPU/mem/IO.  |
| `src/safeboot/SAFEBOOT.EXE`       | Rescue tool for machines that fail a profile.        |
| `src/cfgedit/CFGEDIT.EXE`         | On-device config editor for field fixes.             |
| Hardware bug fixes                | Patches to profiles/drivers found only on metal.      |
| `tests/hardware-signoff.md`       | Per-machine boot/sound/CD/mouse sign-off sheet.       |
| Media-writing docs                | How to image CF cards / floppies for real machines.   |

Tested-hardware matrix shape:

| Machine class | CPU     | RAM  | Video | Sound     | Storage      | Status |
|---------------|---------|------|-------|-----------|--------------|--------|
| Baseline      | 386SX   | 4 MB | VGA   | SB / AdLib| IDE/CF       | gate   |
| Mid           | 386DX   | 8 MB | VGA   | SB Pro    | IDE/CF       | gate   |
| Upper         | 486     | 8–16M| VGA   | SB 16     | IDE/CF       | gate   |
| Early Pentium | P54C    | 16 M | VGA   | SB 16     | IDE          | check  |

### Risks
- **UMB/EMM386 layout differs on real chipsets**, breaking XMS/EMS free-RAM
  targets or hanging boot. *Mitigation:* per-machine `X=` exclusion notes;
  SAFE + CFGEDIT recovery path; conservative defaults.
- **CF card CHS/LBA quirks** prevent boot. *Mitigation:* documented known-good
  cards + partitioning; UIDE tuning.
- **Sound IRQ/DMA conflicts** only visible on metal. *Mitigation:* real-hardware
  conflict matrix; SETSOUND presets adjusted.
- **Access to hardware** (solo maintainer may own few machines). *Mitigation:*
  prioritize the 386SX baseline as the hard gate; treat rarer machines as
  "check" not "gate"; enlist trusted testers before public alpha.

### Exit criteria (objective, testable)
- On a real 386SX (baseline), Castalia installs to CF, cold-boots to the menu,
  and all eight profiles behave per `tests/hardware-signoff.md`.
- On real hardware: at least one game each runs under CLEAN, XMS, EMS, and
  CDROM with working sound and mouse where applicable.
- `HWINFO.EXE` correctly reports CPU class, conventional/XMS/EMS, and detected
  I/O on the baseline machine.
- SAFE + `SAFEBOOT.EXE`/`CFGEDIT.EXE` recover a deliberately broken config on
  metal without external tools.

### Estimated complexity: **XL**
Hardware validation is open-ended: sourcing machines, chasing chipset-specific
bugs, and re-testing after every fix. It is the phase most likely to expose
deep issues, and it depends on physical access and patience.

### Dependencies
Phases 1–6 (a full, installable, feature-complete-for-alpha system to validate).
Consumes and hardens everything before it.

---

## 21.13 Phase 8 — Public alpha

**Codename target:** 1.0-alpha (Tombatossals pre-release)

### Goals
- Release a public alpha image + installer to real users, gather structured
  feedback and bug reports, and stabilize toward 1.0.
- Stand up the contribution and reporting infrastructure (issue templates,
  compatibility-report format, release notes).

### Deliverables (concrete files/artifacts)
| Artifact                              | Purpose                                          |
|---------------------------------------|--------------------------------------------------|
| `dist/castalia-1.0-alpha.img`/`.iso`  | Public alpha media.                              |
| `docs/RELEASE-NOTES.md`               | What's in, what's known-broken.                  |
| `.github/ISSUE_TEMPLATE/*`            | Bug + compatibility-report templates.            |
| `docs/CONTRIBUTING.md`                | How to build, report, add game DB entries.       |
| `docs/KNOWN-ISSUES.md`                | Live list, triaged by machine/profile.           |
| `docs/QUICKSTART.md`                  | 5-minute install-and-boot guide.                 |
| Checksums + signatures                | Integrity for downloaded images.                 |

### Risks
- **Feedback flood the solo maintainer can't triage.** *Mitigation:* templated,
  structured reports (machine, profile, game); label + batch; the Phase 7
  hardware gate means most reports are features/edge-cases, not "won't boot."
- **Reputation risk from a rough alpha.** *Mitigation:* clearly label as alpha,
  ship `KNOWN-ISSUES.md`, set expectations in release notes.
- **Media integrity / trust.** *Mitigation:* publish checksums and signatures.

### Exit criteria (objective, testable)
- Alpha image + installer published with checksums; a fresh user can follow
  `QUICKSTART.md` to boot Castalia on emulator or hardware.
- Issue + compatibility-report templates live and receiving reports.
- A defined triage cadence is in place; a burn-down list of alpha blockers
  exists and is being reduced.
- No open **P0** (boot-blocking on the 386SX baseline) issues remain by the end
  of the alpha window.

### Estimated complexity: **M**
Technically light relative to earlier phases, but the release plumbing,
documentation, and sustained triage load make it more than trivial.

### Dependencies
Phase 7 (validated on hardware — the hard prerequisite). Phases 1–6 for content.

---

## 21.14 Phase 9 — 1.0 release

**Codename target:** 1.0 "Tombatossals"

### Goals
- Ship a stable, documented, legally-clean Castalia DOS 386SX Edition 1.0 that a
  retro user can install and rely on for real DOS gaming.
- Freeze the feature set, finalize documentation and licensing artifacts, and
  cut a reproducible, signed release.

### Deliverables (concrete files/artifacts)
| Artifact                              | Purpose                                          |
|---------------------------------------|--------------------------------------------------|
| `dist/castalia-1.0.img` / `.iso`      | Final 1.0 media (bootable + installer).          |
| `dist/castalia-1.0-src.tar.gz`        | Full source, incl. `third_party/` GPL sources.   |
| `docs/MANUAL.md`                      | Complete user manual (install, profiles, sound, |
|                                       | games, recovery).                                |
| `docs/RELEASE-NOTES.md` (1.0)         | Final notes; supported hardware statement.       |
| `LICENSES/` complete                  | Every license present and correct.               |
| Reproducible-build verification       | Anyone can rebuild the exact image from source.   |
| Signed checksums                      | Release integrity.                               |

Feature set frozen at 1.0 (per project conventions):

| Included at 1.0                                  | Deferred to 1.1+ (see Phase 10) |
|--------------------------------------------------|---------------------------------|
| 8-profile boot menu + memory managers            | File manager `CASTFM.EXE`       |
| `CASTALIA.EXE`, `LAUNCH.EXE`, `MEMPROF.EXE`,      | (aka `CFM.EXE`/`ALCAZAR.EXE`)   |
| `SETSOUND.EXE`, `HWINFO.EXE`, `CFGEDIT.EXE`,      | Roland MT-32 / General MIDI     |
| `SAFEBOOT.EXE`, `GAMECFG.EXE`, `SETUP.EXE`        | Original-kernel research        |
| Installer + directory layout + backups            | Expanded game DB                |
| Curated game compatibility DB seed set            |                                 |

### Risks
- **"1.0" overpromising.** *Mitigation:* scope is explicitly the 386SX baseline
  + validated machines; supported-hardware statement is honest about limits.
- **Licensing gap at release.** *Mitigation:* re-run `tools/verify-licenses.sh`
  and a full `LICENSES/` audit as a release gate; ship GPL sources alongside.
- **Regression from alpha fixes.** *Mitigation:* full `tests/` + hardware
  sign-off re-run on the release candidate before tagging.

### Exit criteria (objective, testable)
- Zero open P0/P1 issues against the 386SX baseline; all `tests/` suites and the
  Phase 7 hardware sign-off pass on the release candidate.
- `tools/verify-licenses.sh` passes; source release includes all GPL component
  sources; `LICENSES/` complete.
- The 1.0 image rebuilds bit-reproducibly from `castalia-1.0-src.tar.gz`.
- A new user can install from media and run a game per `docs/MANUAL.md` without
  external help; release tagged `1.0` and signed.

### Estimated complexity: **M**
No new subsystems — the work is stabilization, documentation, licensing rigor,
and release engineering. Substantial but bounded.

### Dependencies
Phase 8 (public alpha feedback stabilized). Transitively all prior phases.

---

## 21.15 Phase 10 — Future original kernel exploration

**Codename target:** 1.1 "Montornés" and beyond (research track; non-blocking)

### Goals
- Explore, as a *research* effort, whether an **original** DOS-compatible kernel
  (fully owned by the Castalia project, no GPL kernel) is feasible and
  worthwhile — without ever putting the shipped 1.x product at risk.
- In parallel, land the 1.1 user-facing feature that was deferred from 1.0: the
  file manager `CASTFM.EXE` (aka `CFM.EXE` / `ALCAZAR.EXE`).

### Deliverables (concrete files/artifacts)
| Artifact                          | Purpose                                              |
|-----------------------------------|------------------------------------------------------|
| `src/castfm/CASTFM.EXE`           | 1.1 file manager (blue/gray/amber text UI).          |
| `docs/KERNEL-STUDY.md`            | Feasibility study: scope, INT 21h coverage, risk.    |
| `research/kernel/` (spike)        | Throwaway prototypes: boot sector, INT 21h subset.   |
| `docs/COMPAT-TARGETS.md`          | Which DOS APIs a kernel must implement, prioritized. |
| Decision record                   | Go/no-go on continuing original-kernel work.         |

Why this is last and optional:

- An original kernel is an **XL, multi-year, high-failure-rate** undertaking.
  FreeDOS already satisfies 1.0's promise; there is no shipping reason to
  replace it.
- The value would be full ownership and a clean MIT stack top-to-bottom, but the
  cost is re-implementing a huge, compatibility-sensitive API surface (INT 21h,
  FAT, TSR/UMB behaviour, real-mode quirks) that games depend on in undocumented
  ways. **Compatibility beats elegance** argues *for* keeping FreeDOS unless the
  original kernel can match it — which is exactly what the study must determine.

### Risks
- **Bottomless scope / never ships.** *Mitigation:* strictly time-boxed spikes;
  the study produces a go/no-go, not a mandate; 1.x never depends on it.
- **Compatibility regressions** vs FreeDOS. *Mitigation:* any kernel candidate
  must pass the *same* game-compatibility and hardware sign-off suites as 1.0
  before it could ever be considered for shipping.
- **Distraction from maintenance.** *Mitigation:* file manager and bug-fix
  maintenance (the real 1.1 value) take priority over kernel research.

### Exit criteria (objective, testable)
- `CASTFM.EXE` ships in 1.1: browse/copy/move/delete/rename on FAT12/FAT16 in
  the Castalia text UI, passing `tests/filemanager.md`, on emulator + baseline
  hardware.
- `docs/KERNEL-STUDY.md` delivers a concrete, evidence-based go/no-go decision
  with a prioritized INT 21h/DOS-API coverage list and a realistic effort
  estimate.
- If go: a spike in `research/kernel/` boots to a prompt and services a defined
  minimal INT 21h subset in an emulator — as a *proof of concept only*, with no
  impact on the 1.x release line.

### Estimated complexity: **XL**
The kernel study/spike is open-ended research. The 1.1 file manager alone is an
**M**; the combined phase is dominated by the XL kernel exploration.

### Dependencies
Phase 9 (a stable 1.0 must exist first). The kernel track is otherwise
independent and must never block 1.x maintenance or releases.

---

## 21.16 Summary table

| Phase | Name                              | Codename target      | Complexity | Key exit criterion                                                      |
|-------|-----------------------------------|----------------------|------------|-------------------------------------------------------------------------|
| 0     | Research and legal foundation     | pre-0.1              | M          | License audit passes; zero MS artifacts; Watcom builds `hello.exe`.     |
| 1     | Bootable FreeDOS Castalia proto   | 0.1 Almenara         | M          | Image cold-boots to `C:\>` in DOSBox-X + 86Box; `DOS=HIGH`, XMS present.|
| 2     | Memory profiles and boot menu     | 0.2 Peñíscola        | L          | All 8 profiles boot; `MEM` matches matrix (XMS ≥ 620 KB, CLEAN ≥ 615 KB).|
| 3     | Castalia menu and launcher        | 0.2 Peñíscola        | M          | `CASTALIA.EXE` menu navigable; `LAUNCH.EXE` runs a game and returns.    |
| 4     | Sound / CD-ROM / mouse config     | 0.2 Peñíscola        | M          | Valid `SET BLASTER`; CD drive letter; mouse works — all in CDROM profile.|
| 5     | Installer                         | 0.5 Morella          | L          | Unattended install → self-booting menu; emitted config == golden files. |
| 6     | Game compatibility database       | 0.5 Morella          | L          | DB builds + launcher reads it; seed titles run under recommended profiles.|
| 7     | Real hardware validation          | toward 1.0 (pre-alpha)| XL        | Real 386SX installs, boots all 8 profiles, runs games per sign-off sheet.|
| 8     | Public alpha                      | 1.0-alpha Tombatossals| M         | Public media + checksums shipped; no P0 boot-blockers on baseline.      |
| 9     | 1.0 release                       | 1.0 Tombatossals     | M          | Zero P0/P1 on baseline; license audit clean; image rebuilds reproducibly.|
| 10    | Future original kernel exploration| 1.1 Montornés+       | XL         | `CASTFM.EXE` ships in 1.1; kernel study delivers evidence-based go/no-go.|

---

## 21.17 Release contents at a glance

Which original Castalia tools are expected to exist by each codename (cumulative;
all are MIT-licensed originals that talk to GPL FreeDOS components as separate
executables/drivers, never by static-linking):

| Tool           | 0.1 | 0.2 | 0.5 | 1.0 | 1.1 |
|----------------|-----|-----|-----|-----|-----|
| (boot menu)    |  ·  |  X  |  X  |  X  |  X  |
| CASTALIA.EXE   |     |  X  |  X  |  X  |  X  |
| LAUNCH.EXE     |     |  X  |  X  |  X  |  X  |
| MEMPROF.EXE    |     |  X  |  X  |  X  |  X  |
| SETSOUND.EXE   |     |  X  |  X  |  X  |  X  |
| HWINFO.EXE     |     |  ·  |  X  |  X  |  X  |
| CFGEDIT.EXE    |     |     |  X  |  X  |  X  |
| SAFEBOOT.EXE   |     |     |  X  |  X  |  X  |
| GAMECFG.EXE    |     |     |  X  |  X  |  X  |
| SETUP.EXE      |     |     |  X  |  X  |  X  |
| CASTFM.EXE     |     |     |     |     |  X  |

Legend: `X` = present/complete, `·` = present in minimal/stub form, blank = not
yet. `MEMPROF.EXE` appears at 0.2 as the menu's profile switcher; a boot-time
`(boot menu)` in `CONFIG.SYS` exists from 0.2 and is stubbed at 0.1.

---

## 21.18 Cross-cutting practices (all phases)

These hold throughout and are release gates, not afterthoughts:

- **Legal separation** — Castalia MIT code and GPL FreeDOS components stay
  cleanly split (`third_party/`), source shipped, no static-linking across the
  boundary; `tools/verify-licenses.sh` runs in CI every phase.
- **Version honesty** — kernel reports FreeDOS 7.x; Castalia presents
  6.22-*compatible behaviour*; `SETVER` is per-program only. No global spoof.
- **Two-tier testing** — DOSBox-X + 86Box/PCem for iteration; real 386SX/486 for
  sign-off. Nothing reaches the public without an emulator pass, and nothing
  reaches 1.0 without a hardware pass.
- **Lean by construction** — C89, Open Watcom (`wcl -0 -bt=dos -ml -os`), Turbo C
  friendly, fixed buffers, no dynamic allocation, small functions; footprint is
  a measured gate because free conventional RAM is the product.
- **Dignified UX** — blue/gray/amber 16-color VGA text, 80x25, calm and precise
  copy in every screen, error, and help page; never childish.
- **Honesty about limits** — the 386SX baseline is real and constrained; docs
  state supported hardware plainly rather than overpromising.

---

## Addendum A — Application Suite & Games expansion (2026-07)

The suite grew beyond the original tool list; this addendum records the new
scope so nothing is lost. Full design and the classic-DOS-apps research are
in [`APPS.md`](APPS.md).

### Landed (targeting 0.5 "Morella")

| Deliverable | What it is | Source |
|---|---|---|
| `CASTMARK.EXE` | System inspector + benchmark suite: animated gradient bars, real FNSTSW coprocessor probe, CPU/mem/video/disk/FPU marks, saved-score deltas, provisional Castalia Index (386SX/16 = 100) | `src/castmark/` |
| `CASTCOPY.EXE` | Diskette rescue & transfer GUI: tag files on A:/B:, per-file + total progress, live KB/s, read-back verify on by default, partial copies removed on error | `src/castcopy/` |
| `CASTDOC.EXE` | Disk doctor (surface side): INT 13h geometry + floppy-type report, read-only track verify with live map, error-code breakdown, diskette-vs-drive advice | `src/castdoc/` |
| `BANNER.EXE` | Animated boot banner: the castle rises, title shimmer, flag flicker; any key skips; `/Q` for instant | `src/banner/` |
| `SNAKE.EXE`, `PUZZLE.EXE`, `ALMENA.EXE`, `MINAS.EXE` | Castalia minigames (registered in `GAMES.INI`, launchable from LAUNCH): snake, 15-puzzle, the falling-blocks "Almena" (7 pieces, wall-kick rotation, hard drop, line flash, levels), and the "Minas" minesweeper (3 difficulties, safe first reveal, live timer) | `src/games/` |
| CASTALIA.EXE menu v2 | Submenus: System & Benchmark / Disk Tools / Minigames / Configuration — the whole suite reachable from the front end | `src/castalia/` |
| `HELP.EXE` | The help-system reader designed in HELP.md, finally implemented: HELP.IDX topic index (shared INI), full-screen pager, `HELP <topic>` prefix jump; wired to the menu's Help item | `src/help/` |
| `CASTEDIT.EXE` | General text editor (QEdit-class ambition, line-oriented engine): any file, `.BAK` on every save, save-as, new-file support; menu item 3 | `src/castedit/` |
| CI pipeline + tests | check gate → host unit tests (INI, 39 checks) → real Open Watcom build with MZ verification → E2E: FreeDOS payload fetch, SMOKE.EXE inside headless DOSBox, bootable image build, and a real **boot test** asserting the AUTOEXEC marker. Floppy-specific CONFIG.SYS/AUTOEXEC.BAT added (the HDD `SHELL=C:\...` config could not boot from A: — found by the boot e2e); HDD templates ride in `::/INSTALL/` and SETUP prefers them | `.github/workflows/ci.yml`, `scripts/`, `tests/`, `config/floppy/` |
| `CPUDET`, `DIRW` common modules | Shared CPU/FPU probes (compiled `-3`) and portable dir scanning; HWINFO/CASTFM refactored onto them | `src/common/` |

### Addendum A.2 — the Castalia kernel and the second wave (2026-07)

| Deliverable | What it is | Source |
|---|---|---|
| **The Castalia kernel** | The rebuilt FreeDOS kernel stopped being a renamed banner and became a real derivative: Castalia OEM id (`0xCA`) in `INT 21h`/`AH=30h`, a resident boot-profile byte fed by a new `CASTALIA=` `CONFIG.SYS` directive, a boot-tick stamp for true uptime, and an `INT 2Fh`/`AH=0CAh` identity multiplex (`CA00h` identity, `CA01h` profile, `CA02h` uptime) that the tools query live. Full register contract in [`KERNEL.md`](KERNEL.md) | `scripts/patch-kernel-src.py`, `scripts/build-kernel.sh` |
| `CASTID.EXE` | The kernel signature card: build, edition, OEM id, live boot profile and uptime read straight from the kernel, plus CPU/memory/clock — honest about a stock kernel when it finds one | `src/castid/` |
| `CDPLAYER.EXE` | Red Book audio CD player over MSCDEX (`INT 2Fh` `1500h`/`1510h`): TOC read, play/pause/stop/skip, live Q-channel time, graceful when no driver or no audio disc | `src/cdplayer/` |
| `SAVER.EXE` | The "Castalia at night" screensaver: starfield, drifting moon, candle-lit keep windows, shooting stars, drifting wordmark | `src/saver/` |
| `SIEGE.EXE`, `REVERSI.EXE`, `BARRELS.EXE`, `SOLITARE.EXE` | Four more minigames: the catapult duel (integer-ballistics, adaptive gunner), Othello against a depth-4 alpha-beta opponent, fortress-cellar Sokoban with undo, and draw-one Klondike | `src/games/` |
| Menu v3 + a living gate | The whole suite reachable from `CASTALIA.EXE`; submenu counts derived with `sizeof` so they cannot drift; the main screen now breathes (twinkling field, candlelight and fluttering pennants on the crown) via new `logo_mark_windows()` / `logo_mark_flags()` hooks in the shared logo module | `src/castalia/`, `src/common/LOGO.C` |
| `CASTTOUR.EXE`, `CASTLINK.EXE` | The interactive animated guided tour (also an attract mode), and the LapLink-style serial transfer this addendum had listed as "next" | `src/casttour/`, `src/castlink/` |
| `SPK` — the suite's voice | The PC-speaker code lifted out of SETSOUND into `src/common/SPK.{C,H}`, with the non-blocking `spk_note()`/`spk_poll()` pair a game loop needs, the house voices, and three levers for silence (`CASTSOUND`, a per-game `S` key, `spk_off()` on exit). See [`SOUND.md`](SOUND.md) | `src/common/SPK.C` |
| `UNDEL.EXE` | The APPS.md "undelete, read-only-first" item, taken literally: it never writes to the disk it is recovering. Reads with INT 21h 7305h / IOCTL 440Dh (both read-only and stack-clean), judges each deleted entry as good / partly overwritten / nothing to recover, and copies the data out to a *different* drive — a same-drive destination is refused. Unformat stays out of scope: it could only work by writing to the damaged disk. Both the root-directory scan and the recovery read in 8 KB strides rather than one sector per DOS call, and a run that fails is retried sector by sector so a single bad sector costs one entry instead of the rest of the scan | `src/undel/` |
| Info-ZIP bundled, license-clean | `fetch-payload.sh --with-archiver` stages Info-ZIP `UNZIP`/`ZIP` plus a DPMI host for the hard-disk distribution (they do not fit the 1.44 MB rescue floppy). FreeDOS's UnZip turned out to be a DJGPP build needing DPMI, so CWSDPMI rides along and the script asserts that ZIP is still the real-mode build. `LICENSES/MANIFEST.md` is now populated and `fetch-payload.sh` records SHA256 provenance for every archive it downloads, closing the Phase 0 checksum requirement | `scripts/fetch-payload.sh`, `LICENSES/` |
| CI survives its mirrors | ibiblio began answering 403 for `/files/distributions/` while `/files/repositories/` stayed up, which reddened E2E. `fetch-payload.sh` now falls back to assembling the DOS core from packages and **assembles the FAT12 boot sector from the kernel's own `boot/boot.asm` with nasm** instead of lifting it from a shipped image — cleaner for licensing, and it needs no binary distribution at all | `scripts/fetch-payload.sh`, `.github/workflows/ci.yml` |

### Next (1.0 → 1.1 candidates)

| Item | Notes |
|---|---|
| Calibrate `CASTMARK` baselines on real hardware | 386SX/16 must land near index 100; record runs in `APPS.md` |
| Hardware pass for the new kernel calls | `CA00h`/`CA01h`/`CA02h` and the `0xCA` OEM id verified on metal, not just in the emulator |
| Exercise `CDPLAYER` on a real drive | the MSCDEX path is written to spec but has never met a physical CD |
| ~~More minigames (solitaire)~~ | done — `SOLITARE.EXE` |
| ~~Migrate legacy inline editors~~ | done — CFGEDIT/GAMECFG now use the shared `ui_editline` |

### Explicitly rejected

- Sidekick-style TSR popups (violates the no-TSR performance rule).
- SpinRite-class low-level refresh (data-risk beyond our testing means).
