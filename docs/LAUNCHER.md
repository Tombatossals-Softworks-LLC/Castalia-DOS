# 7 & 8. Game Launcher and Main Menu

This document covers two of the user-facing text-mode programs: the game
launcher (`LAUNCH.EXE`, Section 7) and the main menu (`CASTALIA.EXE`,
Section 8). The working prototypes are in
[`src/launch/LAUNCH.C`](../src/launch/LAUNCH.C) and
[`src/castalia/CASTALIA.C`](../src/castalia/CASTALIA.C); they share the INI
reader and text-mode UI library in [`src/common/`](../src/common/).

---

# 7. Game Launcher — `LAUNCH.EXE`

**Alternative name:** `GAMEVAULT.EXE` (the same program can ship under either
name).

A text-mode DOS program that lets the user browse, inspect, and launch games.
It runs on a 386SX, is keyboard-first (mouse optional and never required),
reads its game list from an INI file, and — crucially — **gets out of memory
before the game runs**.

## Requirements met

| Requirement | How |
|---|---|
| Runs on 386SX | 80×25 text mode, fixed buffers, single-pass INI parse |
| Text mode, keyboard first | direct video writes; `INT 16h` keys; mouse optional |
| Reads game definitions from INI | `GAMES.INI` via the shared `INI` reader |
| Per-game memory profile recommendation | `profile=` key; warns on mismatch |
| Sound profile | `sound=` key shown in detail pane |
| Executable path + launch arguments | `path=`, `exe=`, `args=` keys |
| Notes | `notes=` key, word-wrapped in the detail pane |
| Compatibility flags | `requires_cd=`, `mouse=` (extensible) |
| Pre-launch warnings | profile mismatch and CD-profile warnings |
| Backup of config files | handled by the main menu (Backup System Config) |
| Recommend rebooting into another profile | mismatch dialog offers a "note" |

## Data format (`GAMES.INI`)

One `[SECTION]` per game; a section is a game if it has an `exe` key.

```
[WOLF3D]
name        = Wolfenstein 3D
path        = C:\GAMES\WOLF3D
exe         = WOLF3D.EXE
args        =
profile     = XMS
sound       = SBPRO
mouse       = no
requires_cd = no
notes       = Runs well on a fast 386 or better.

[MONKEY]
name        = The Secret of Monkey Island
path        = C:\GAMES\MONKEY
exe         = MONKEY.EXE
profile     = CLEAN
sound       = ADLIB
mouse       = yes
requires_cd = no
notes       = Use the CLEAN profile if you hit sound problems.
```

| Key | Meaning | Default |
|---|---|---|
| `name` | Display name | the section id |
| `path` | Game directory (with drive) | current dir |
| `exe` | Executable to run | *(required)* |
| `args` | Command-line arguments | empty |
| `profile` | Recommended profile (CLEAN/XMS/EMS/CDROM) | XMS |
| `sound` | Sound hint (NONE/SPKR/ADLIB/SB/SBPRO/SB16) | NONE |
| `mouse` | Uses a mouse (yes/no) | no |
| `requires_cd` | Needs the CDROM profile (yes/no) | no |
| `notes` | Free text shown in the detail pane | empty |

The full shipping database is [`config/GAMES.INI`](../config/GAMES.INI).

## Text-mode UI mockup (80×25)

```
 CASTALIA DOS  Game Launcher                            Profile: XMS
 ┌ Games ──────────────────────────┐ ┌ Details ──────────────────────────┐
 │ Prince of Persia                │ │ Wolfenstein 3D                     │
 │ Commander Keen 4                │ │                                    │
 │ The Secret of Monkey Island     │ │ Path : C:\GAMES\WOLF3D             │
 │ Indiana Jones - Atlantis        │ │ Exe  : WOLF3D.EXE                  │
 │ Sid Meier's Civilization        │ │ Sound: SBPRO                       │
 │ Dune II                         │ │ Mouse: no   CD-ROM: no             │
 │ Wolfenstein 3D                ◄ │ │                                    │
 │ Stunts                          │ │ Recommended profile: XMS           │
 │ Lemmings                        │ │                                    │
 │ X-COM - UFO Defense             │ │ Notes:                             │
 │ Doom                            │ │ Runs well on a fast 386 or better. │
 │ SimCity Classic                 │ │                                    │
 │ The 7th Guest                   │ │                                    │
 │                                 │ │                                    │
 └─────────────────────────────────┘ └────────────────────────────────────┘
  ↑↓ Move   Enter Launch   F1 Help   Esc Quit
```

The selected game is drawn as a black-on-amber bar. The detail pane colours
the recommended profile **green** when it matches the booted profile and
**amber** when it does not, with a hint line naming the current profile.

## Launcher flow

```
start
  read %CASTPROFILE% (booted profile)
  load GAMES.INI (try C:\CASTALIA\CFG, then local fallbacks)
  if no games -> show a friendly "no games" detail pane
  loop:
    draw list + detail + status
    key:
      Up/Down/PgUp/PgDn/Home/End -> move selection
      F1    -> help overlay
      Esc   -> exit(0)  (return to the Castalia menu)
      Enter -> confirm_launch(selected)
  confirm_launch:
    build full exe path (path + '\' + exe)
    if exe missing -> error dialog, cancel
    if requires_cd and booted != CDROM -> CD warning dialog
    else if profile != booted         -> mismatch dialog (Enter/R/Esc)
    else                              -> plain confirm dialog
    on confirm -> gen_runbatch(); exit(77)
  gen_runbatch:
    write _RUNGAME.BAT: change drive, CD path, run exe [args], CD C:\
```

## Pseudocode (core loop)

```
sel = 0; top = 0
forever:
    if sel < top: top = sel
    if sel >= top + LIST_ROWS: top = sel - LIST_ROWS + 1
    draw_list(sel, top); draw_detail(sel)
    key = getkey()
    switch key:
        UP:    if sel>0 sel--
        DOWN:  if sel<n-1 sel++
        PGUP:  sel = max(0, sel-LIST_ROWS)
        PGDN:  sel = min(n-1, sel+LIST_ROWS)
        HOME:  sel = 0
        END:   sel = n-1
        F1:    show_help(); redraw
        ESC:   return 0
        ENTER: if confirm_launch(sel) and gen_runbatch(sel): return 77
               redraw
```

## Why the batch handoff (memory discipline)

A launcher that spawns the game with `system()` or `spawnl()` stays resident,
stealing 30–60 KB of conventional memory from the game. On a 386SX that can be
the difference between a game running and refusing to start. So `LAUNCH.EXE`
does **not** run the game. It writes a one-shot `_RUNGAME.BAT` and exits with
errorlevel 77; the wrapper `GAMES.BAT` then runs that batch after the launcher
has completely left memory:

```
GAMES.BAT:
    LAUNCH.EXE
    IF ERRORLEVEL 78 GOTO done
    IF ERRORLEVEL 77 CALL C:\CASTALIA\CFG\_RUNGAME.BAT
```

```
_RUNGAME.BAT (generated):
    @ECHO OFF
    C:
    CD "\GAMES\WOLF3D"
    WOLF3D.EXE
    C:
    CD \
```

This is the professional DOS idiom for a launcher and is why both the launcher
and the main menu route game launches through batch files rather than spawning.

## Compatibility-flag extension

`requires_cd` and `mouse` are booleans today. The format is designed to grow:
future flags (`no_emm`, `needs_ems`, `slowdown`, `dpmi`, `vesa`) map onto the
compatibility flags in [`COMPATIBILITY.md`](COMPATIBILITY.md) and can drive
richer pre-launch warnings without changing the file structure.

## Editing the game database

Games can be added and edited without hand-editing `GAMES.INI`. Pressing **E**
in the launcher runs the game-database editor `GAMECFG.EXE`
([`../src/gamecfg/GAMECFG.C`](../src/gamecfg/GAMECFG.C)); when it exits, the
launcher reloads the list so new games appear immediately. `GAMECFG` presents
a list plus a form (text fields via an inline editor; profile and sound by
cycling; mouse/CD as yes/no), backs the file up to `C:\CASTALIA\BACKUP` before
saving, and rewrites the whole file. It can also be run directly from the
command line (`GAMECFG`).

---

# 8. Main Menu — `CASTALIA.EXE`

The main user-facing front end, started from `AUTOEXEC.BAT` unless the user
booted "Command Prompt Only." It should feel like professional early-1990s
software: calm, readable, keyboard-driven, never childish.

## Menu items (v2 — submenus for the grown suite)

1. Launch Games *(batch handoff to the launcher)*
2. File Manager *(CASTFM)*
3. Text Editor *(CASTEDIT)*
4. System & Benchmark ▸ — System Benchmark (CASTMARK), Hardware
   Diagnostics (HWINFO), Memory Profiles (MEMPROF)
5. Disk Tools ▸ — Diskette Rescue / Copy (CASTCOPY), Disk Doctor (CASTDOC)
6. Minigames ▸ — Snake, 15-Puzzle, Almena, Minas (minesweeper)
7. Sound Setup *(SETSOUND; CD-ROM support is a boot-profile matter, see
   `MEMORY.md`)*
8. Configuration ▸ — Edit CONFIG.SYS / AUTOEXEC.BAT (CFGEDIT), Game
   Database Editor (GAMECFG), Backup System Config, Restore System Config
9. Help *(runs the HELP.EXE topic reader; About box when not installed)*
Q. Exit to Command Line

Submenu model: Enter (or →) opens the panel beside the main menu; Esc (or ←)
closes it; number keys are hotkeys inside each panel. Running a tool returns
to the same submenu, so several tools can be used in a row.

## Text-mode UI mockup (80×25)

```
 CASTALIA DOS                                    Sat 2026-07-11  14:32:07

                      ┌─┐   ┌─┐   ┌─┐
                      │ │███│ │███│ │
                      ├───────────────┤

                            CASTALIA DOS
                            386SX Edition
                       Version 1.0 "Tombatossals"

             ┌ Main Menu ──────────────────────┐
             │  1   Launch Games               │
             │  2   File Manager               │
             │  3   Text Editor                │
             │  4   System & Benchmark       ► │
             │  5   Disk Tools               ► │──┌ Disk Tools ─────────┐
             │  6   Minigames                ► │  │ 1  Diskette Rescue  │
             │  7   Sound Setup                │  │ 2  Disk Doctor      │
             │  8   Configuration            ► │  └─────────────────────┘
             │  9   Help                       │
             │  Q   Exit to Command Line       │
             └─────────────────────────────────┘

 Profile: XMS    ↑↓ Move   Enter Select   Esc Exit
```

The top-right corner carries a **live wall clock** (`Www YYYY-MM-DD  HH:MM:SS`,
the date in white and the time in amber) read straight from the DOS clock, so
the front end feels awake. While it waits for a key it ticks about twice a
second and issues the DOS idle interrupt (INT 28h), so it never spins a real
386 - or a DOSBox/86Box core - at 100%.

## Keyboard navigation model

| Key | Action |
|---|---|
| ↑ / ↓ | Move the selection (wraps top/bottom) |
| Home / End | First / last item |
| `1`–`9`, `0`, `Q` | Jump straight to an item |
| Enter | Activate the selected item |
| Esc | Exit to the command line |

## Colour scheme (VGA text)

The Castalia palette, defined once in [`src/common/UI.H`](../src/common/UI.H):

| Element | Foreground | Background | Attribute |
|---|---|---|---|
| Desktop field | light grey (7) | blue (1) | `A_DESKTOP` |
| Title bar | amber (14) | blue (1) | `A_TITLE` |
| Frame lines | white (15) | blue (1) | `A_FRAME` |
| Menu item | light grey (7) | blue (1) | `A_ITEM` |
| Selected item | black (0) | amber (14) | `A_ITEMSEL` |
| Dialog/panel body | black (0) | light grey (7) | `A_PANEL` |
| Panel heading | white (15) | light grey (7) | `A_PANELHDR` |
| Status bar | black (0) | light grey (7) | `A_STATUS` |
| Warning | amber (14) | red (4) | `A_WARN` |

Blue field, grey/white panels, amber trim, black-on-amber selection: serious,
legible, and comfortable on a 16-colour VGA text screen. See
[`BRANDING.md`](BRANDING.md) for the full identity.

## How actions behave

- **Launch Games** — exits with errorlevel 10 so the `AUTOEXEC.BAT` loop runs
  the launcher (via `GAMES.BAT`) with the menu out of memory.
- **In-place tools** (Sound Setup, Diagnostics, Config Editor, File Manager) —
  run with `system()` from `C:\CASTALIA\BIN`; if a tool is not installed, a
  polite "not installed" dialog appears instead of an error.
- **Backup / Restore System Config** — copy `CONFIG.SYS`/`AUTOEXEC.BAT` to and
  from `C:\CASTALIA\BACKUP` (creating the folder if needed), then report the
  result.
- **Help** — an About/Help overlay; the command-line `HELP` opens the full
  help system (see [`HELP.md`](HELP.md)).
- **About / Credits** — `F1` (or Help, uninstalled) opens the About panel;
  pressing **C** there rolls the animated **credits** — a demoscene-style
  scroll of the wordmark, the studio, and both creators over a starfield.
  It is also reachable directly with `CASTALIA /CREDITS` (and `CASTALIA /VER`
  still prints the one-line version).
- **Exit to Command Line** — returns errorlevel 0; the loop ends at the prompt.

## File / folder layout used by these tools

```
C:\CASTALIA\BIN\   CASTALIA.EXE  LAUNCH.EXE  GAMES.BAT  (+ other tools)
C:\CASTALIA\CFG\   CASTALIA.INI  PROFILES.INI  GAMES.INI  SOUND.BAT
                   _RUNGAME.BAT  (transient, written by the launcher)
C:\CASTALIA\BACKUP\ CONFIG.SYS  AUTOEXEC.BAT  (copies)
C:\GAMES\          the games themselves
```

## INI configuration format

`CASTALIA.EXE` reads `[about]` (edition, version, codename) to fill its banner
and About box, and `[launcher]` for defaults. The full file is
[`config/CASTALIA.INI`](../config/CASTALIA.INI). The launcher reads
`GAMES.INI`; both use the shared reader, so the format is identical everywhere:
`[section]`, `key = value`, `;` comments, case-insensitive names.
