# 13. Documentation System

> **Implementation status:** `HELP.EXE` is implemented at
> [`../src/help/HELP.C`](../src/help/HELP.C) (`wmake help`): topic index
> from `HELP.IDX` (shared INI reader), full-screen pager, `HELP <topic>`
> jump with prefix matching, and dev-tree path fallbacks. The Castalia
> menu's Help item runs it when installed (About box as fallback).

CASTALIA DOS ships an on-disk help system so a user with no manual and no
internet can still learn the system from the machine itself. It has two parts:

1. **`HELP.EXE`** — a small text-mode reader that lists topics and displays
   plain-text pages with paging.
2. **The pages** — plain `.TXT` files in `C:\CASTALIA\HELP`, each written to
   fit an 80×25 screen (≤ 78 columns of text, ≤ 22 body lines per screen so a
   title bar and a footer fit).

## `HELP.EXE` design

- **Purpose:** browse and read the help pages without leaving DOS.
- **Invocation:** `HELP` (topic index) or `HELP SOUND` (jump to a topic).
- **UI:** a left topic list and a right reader pane, or a full-screen reader
  for a chosen page; `PgUp`/`PgDn` to page, arrows to move, `Esc` to exit.
- **Implementation:** reuses the shared `UI` library; reads each page with the
  standard C library into a fixed line buffer and pages it. No formatting
  engine — the pages are pre-wrapped plain text, which is fast on a 386SX and
  trivial to author and translate.
- **Topic index:** `HELP.EXE` reads `C:\CASTALIA\HELP\HELP.IDX`, a small INI
  mapping topic ids to titles and filenames, so pages can be added without
  recompiling.

**`HELP.IDX` format** — one `[id]` section per topic, with the keys on
their own lines (the shared INI reader treats a `[section]` line purely
as a header, so keys placed inline on it would be lost):
```
[START]
title=Getting Started
file=START.TXT
[SOUND]
title=Configuring Sound Blaster
file=SOUND.TXT
; ... one block per topic (12 in the shipped index)
```

## Reader mockup (80×25)

```
 CASTALIA DOS Help                                    Boot Profiles   1/2
 ┌──────────────────────────────────────────────────────────────────────┐
 │ CASTALIA DOS gives you eight boot choices. Pick one at the menu when  │
 │ the machine starts; the default (XMS) is chosen after 10 seconds.     │
 │                                                                        │
 │   CLEAN   Maximum compatibility. Loads almost nothing.                │
 │   XMS     Best all-round gaming profile. The default.                 │
 │   EMS     For games that ask for expanded memory.                     │
 │   CDROM   For CD-ROM games. The CD appears as drive D:.               │
 │   WIN3X   For Windows 3.1.                                            │
 │   DIAG    Boot then show the hardware report.                         │
 │   SAFE    Bare system for repairs.                                    │
 │   PROMPT  Straight to a command prompt.                               │
 │                                                                        │
 └──────────────────────────────────────────────────────────────────────┘
   PgUp/PgDn Page   ↑↓ Scroll   Esc Topics
```

## Topic set

The help system covers every task a user meets, matching this bible:

| Topic | Page | Mirrors bible section |
|---|---|---|
| Getting Started | `START.TXT` | Vision / this doc |
| Boot Profiles | `PROFILES.TXT` | Memory (§6) |
| Installing Games | `GAMES.TXT` | Launcher (§7) |
| Configuring Sound Blaster | `SOUND.TXT` | Sound (§9) |
| Configuring the Mouse | `MOUSE.TXT` | Architecture (§4) |
| CD-ROM Setup | `CDROM.TXT` | Memory / Architecture |
| Memory Troubleshooting | `MEMORY.TXT` | Memory (§6) |
| Common Game Problems | `PROBLEMS.TXT` | Compatibility (§3) |
| The CONFIG.SYS Guide | `CONFIG.TXT` | Boot (§5) |
| The AUTOEXEC.BAT Guide | `AUTOEXEC.TXT` | Boot (§5) |
| Recovery Guide | `RECOVERY.TXT` | Boot / Install |
| Legal and Licenses | `LEGAL.TXT` | Licensing (§2) |

## Sample help pages

These are ready to install to `C:\CASTALIA\HELP`. Each fits 80×25.

### `START.TXT`

```
GETTING STARTED WITH CASTALIA DOS
=================================

Welcome. CASTALIA DOS is a DOS system tuned for games on older PCs.

The basics:

 * When the machine starts, you see a menu of boot profiles. If you
   are not sure, just wait - the XMS profile is chosen for you and is
   right for most games.

 * After booting you get the Castalia menu. From here you can launch
   games, set up sound, check your hardware, and more. Use the arrow
   keys and Enter, or press the number next to an item.

 * To add games, put each game in its own folder under C:\GAMES and
   add a few lines to C:\CASTALIA\CFG\GAMES.INI. See "Installing
   Games" (type: HELP GAMES).

 * If a game will not run, try the CLEAN profile (reboot and pick 1),
   which loads the least and frees the most memory.

To read any topic, type HELP followed by its name, for example:
   HELP SOUND      HELP MEMORY      HELP RECOVERY
```

### `SOUND.TXT`

```
CONFIGURING SOUND BLASTER
=========================

DOS games find your sound card by reading the BLASTER variable. It
looks like this:

   SET BLASTER=A220 I5 D1 H5 T4

   A220  card at I/O port 220 (hex)   I5  interrupt (IRQ) 5
   D1    8-bit DMA channel 1          H5  16-bit DMA channel 5
   T4    card type (4 = SB Pro, 6 = SB16)

CASTALIA DOS sets this for you from C:\CASTALIA\CFG\SOUND.BAT. To
change it, run Sound Setup from the Castalia menu (item 4), or edit
SOUND.BAT by hand.

Most cards use A220 I5 D1. If you hear nothing:
 * make sure the card's own jumpers/software match these numbers
 * try IRQ 7 instead of 5 (I7)
 * for a plain Sound Blaster (not Pro/16), use T3 and drop H5

In a game's own setup, choose "Sound Blaster" and the same port/IRQ.
For music, choose "AdLib" or "Sound Blaster" - both use the FM chip.

See also: HELP PROBLEMS
```

### `MEMORY.TXT`

```
MEMORY TROUBLESHOOTING
======================

Symptom: a game says "not enough memory" or refuses to start.

1. Check how much you have. At the prompt type:
      MEM /C /P
   Look at "Largest executable program size". That is what the game
   can use. Compare it to what the game needs.

2. Use a leaner profile. Reboot and pick:
      1 CLEAN  - loads the least, frees the most
      2 XMS    - almost as much free memory, plus a mouse
   Avoid EMS and CD-ROM profiles unless the game needs them; their
   page frame and drivers use memory.

3. Free the sound TSR. If you do not need sound for setup, a leaner
   profile skips it.

Symptom: a game wants EMS ("expanded memory").
   Reboot and pick 3 (EMS). That profile provides a 64 KB EMS page
   frame. Switch back to XMS afterwards for other games.

Symptom: a game misbehaves with a memory manager.
   Pick 1 (CLEAN) or, as a last resort, 7 (SAFE), which loads no
   memory manager at all.
```

### `RECOVERY.TXT`

```
RECOVERY GUIDE
==============

If the machine will not boot, or a change broke something:

 * At the boot menu, pick 7 (Safe Mode). It loads no drivers and
   almost always starts. From the prompt you can run:
      SAFEBOOT   restore a known-good CONFIG.SYS / AUTOEXEC.BAT
      CFGEDIT    edit the boot files safely
      HWINFO     check what the machine sees

 * If even Safe Mode will not start, boot the CASTALIA DOS emergency
   floppy. Then run SAFEBOOT to restore your files from
   C:\CASTALIA\BACKUP, or re-make the disk bootable with SYS C:.

 * Your CONFIG.SYS and AUTOEXEC.BAT are backed up automatically by
   Setup and by the "Backup System Config" menu item. Restores come
   from C:\CASTALIA\BACKUP.

Nothing in recovery touches your games or data in C:\GAMES.
```

### `LEGAL.TXT`

```
LEGAL AND LICENSES
==================

CASTALIA DOS is built on FreeDOS and other open-source software, plus
original Castalia tools and documentation.

 * The DOS core (kernel and shell) is FreeDOS, licensed under the GNU
   General Public License. Its source is included with this release.

 * The Castalia tools are original work under the MIT license.

 * The documentation is licensed CC BY 4.0.

 * CASTALIA DOS contains no Microsoft code, text, or branding. It is
   not MS-DOS and is not affiliated with Microsoft.

Full license texts are in C:\CASTALIA\HELP\LICENSES and in the
LICENSES folder of the source release. See "docs/LICENSE-STRATEGY.md"
in the source for details.
```

## Authoring rules for help pages

- Wrap text at 72–78 columns; never rely on the reader to wrap.
- One idea per screen; keep a page to ≤ 44 lines (two screens) where possible.
- Plain language, imperative voice, concrete keystrokes ("press 1", "type
  MEM /C").
- No hype and no jargon without a one-line definition.
- Cross-reference with `HELP TOPIC` so the reader can hop between pages.

## Tone

The help system speaks the way the rest of CASTALIA DOS does: calm, precise,
and warm — a knowledgeable friend, not a marketing brochure and not a stack
trace. See [`BRANDING.md`](BRANDING.md) for the voice guide.
