# 24. Future Graphical Shell (Post-1.0)

> **Status: OUT OF 1.0 CORE.** This section is a forward-looking design note,
> not a 1.0 deliverable. CASTALIA DOS 1.0 "Tombatossals" ships a text-mode UI
> (VGA 80x25, 16 colors) and nothing else. The graphical shell described here is
> an **optional, separately-shipped module** that a user may install and launch
> *on demand*. It is never resident by default, never required to run a game,
> and never on the boot path. If this module were deleted from a machine, every
> other part of CASTALIA DOS would behave identically. That property is the whole
> point.

This document exists so the idea is captured, scoped, and fenced off — so that
"we should build a desktop" is a decision already reasoned about, not a temptation
that quietly bloats the 1.0 core.

---

## 24.1 Name

Four candidates were considered:

| Candidate           | Reading                                  | Verdict                         |
|---------------------|------------------------------------------|---------------------------------|
| **CASTALIA DESK**   | Plain, on-brand, says exactly what it is | **Recommended**                 |
| ALCAZAR DESKTOP     | Evocative fortress imagery               | Collides with existing name     |
| CASTALIA GEM        | Nods to Digital Research GEM             | Trademark/heritage risk         |
| CASTALIA SHELL      | Generic; "shell" already means COMMAND.COM | Ambiguous, avoid              |

**Recommendation: CASTALIA DESK.**

Reasoning:

- **ALCAZAR is already taken.** Per the tools list, the 1.1 file manager
  `CASTFM.EXE` carries the alternate name `ALCAZAR.EXE`. Naming the desktop
  "ALCAZAR DESKTOP" would put two unrelated products under the same fortress
  word and confuse users and the directory layout. The alcázar imagery is strong
  but it belongs to the file manager. We keep it there.
- **CASTALIA GEM** borrows equity from Digital Research's GEM. GEM is a real,
  historically-loaded product name; leaning on it invites confusion and possible
  mark disputes. We are inspired by GEM (see §24.3), we do not brand as GEM.
- **CASTALIA SHELL** overloads "shell," which in DOS already means the command
  interpreter (`COMMAND.COM`, `SHELL=` in `CONFIG.SYS`). A graphical program
  manager is not a shell in that sense. Using the word invites misfiled bug
  reports and documentation collisions.
- **CASTALIA DESK** keeps the flagship brand in front, reads as a workspace/desk
  (a program-manager surface, not a full OS), and leaves the `ALCAZAR` and `GEM`
  words free of baggage. It is the safe, honest, on-brand choice.

Working executable and layout:

| Item              | Value                                                     |
|-------------------|-----------------------------------------------------------|
| Program           | `DESK.EXE` (aka `CDESK.EXE`)                               |
| Install location  | `C:\CASTALIA\DESK\` (its own subtree — a removable module) |
| On default PATH?  | **No.** Invoked explicitly, e.g. `DESK` after adding it    |
| Ships with core?  | **No.** Separate optional download / install-time opt-in   |

---

## 24.2 Design principles

CASTALIA DESK is bound by the same values as the rest of CASTALIA DOS. It is a
convenience layer, never a gatekeeper.

1. **Optional.** Not installed by default. A first-class CASTALIA DOS machine
   never has it.
2. **On-demand.** Launched by the user (`DESK` at the prompt, or a menu entry in
   `CASTALIA.EXE`). It runs, the user does something, it exits.
3. **Not resident.** It installs no TSR, hooks no timer permanently, and leaves
   nothing in memory after exit. When it is not running, it costs zero bytes of
   conventional RAM.
4. **Not required for compatibility.** No game, tool, or driver depends on it.
   Games are launched *from DOS*, with DESK fully unloaded (see §24.4).
5. **VGA/EGA aware.** Detects the adapter and picks a mode it can actually drive.
   Degrades to text mode on hardware or on user preference.
6. **Lightweight.** Must fit and *feel usable* on a 386SX/16 with a few MB of
   RAM and an ISA VGA card. If it needs a 486 to be pleasant, it has failed its
   brief.
7. **16-color.** Matches the house palette: blue field, gray/white panels,
   amber/yellow highlights, black-on-amber selection. Same visual identity as the
   text UI, rendered in pixels instead of characters.
8. **Familiar, honestly borrowed.** Inspired by the *interaction models* of
   Windows 3.x Program Manager, Digital Research GEM, Geoworks/GEOS (Ensemble),
   and IBM's plain, serious utility screens. We copy no code, art, or names from
   any of them — only the well-worn idioms (a desktop field, framed windows,
   iconized program groups, a menu bar).

---

## 24.3 High-level architecture

CASTALIA DESK is a single small real-mode program built with the same toolchain
as everything else (Open Watcom / Turbo C friendly, 16-bit, size-optimized,
fixed buffers, minimal dynamic allocation). It is deliberately simple: a program
manager, not a multitasking GUI. No preemption, no overlapping-window compositor
with clipping trees beyond what a region list needs, no cooperative task switching
of DOS programs. One thing at a time.

### 24.3.1 Event loop

A classic single-threaded event pump:

```
init_video();          /* pick mode; save prior mode for clean restore   */
load_launcher_data();  /* parse GAMES.INI + PROFILES.INI into memory      */
draw_desktop();        /* field, menu bar, program-group windows          */
while (running) {
    ev = get_event();  /* poll mouse + BIOS keyboard; coalesce mouse move */
    switch (ev.type) {
        case EV_MOUSE_MOVE:  move_pointer(ev.x, ev.y);            break;
        case EV_MOUSE_DOWN:  hit_test_and_focus(ev.x, ev.y);      break;
        case EV_MOUSE_DBLCLK: activate_item(ev.x, ev.y);         break;
        case EV_KEY:         dispatch_key(ev.key);                break;
        case EV_QUIT:        running = 0;                         break;
    }
    if (dirty) repaint_dirty_regions();  /* dirty-rectangle redraw only    */
}
restore_video();       /* return to the exact prior text mode             */
```

No timer TSR: the pointer is polled and drawn inside the loop, so nothing stays
hooked after exit. `get_event()` blocks on nothing longer than one mouse/keyboard
poll, keeping CPU cost predictable on a slow 386SX.

### 24.3.2 Minimal windowing / region model

- **Window = a rectangle + a title + a content type.** Content types are few:
  *program group* (a grid of program items), *dialog* (a message + buttons), and
  *desktop field* (the backdrop).
- **Z-order is a short list**, not a tree. With only a handful of program-group
  windows plus at most one modal dialog, a linear list is enough.
- **Dirty rectangles, not full repaints.** Moving a window or the pointer marks
  affected rectangles; only those are redrawn. This is the single most important
  performance choice on a 386SX with slow planar VGA writes (see §24.4).
- **The mouse pointer is XOR-drawn or save-under**, so it can move without
  forcing a background repaint. A 16x16 save-under buffer is cheap.
- **No arbitrary clipping regions.** Windows are non-overlapping in the common
  case (program groups tile); overlap is handled by simple front-to-back
  rectangle subtraction. If that ever proves too limiting, it is a deliberate,
  reviewed increase in scope — not a default.

### 24.3.3 Input: mouse-driven, keyboard fallback

- **Mouse via `CTMOUSE`** (already a core component; serial + PS/2, 2-clause
  BSD). DESK talks to it through INT 33h — it does **not** bundle its own mouse
  driver. If no mouse is present, DESK is still fully operable by keyboard.
- **Keyboard is a first-class fallback, never an afterthought.** Tab / Shift-Tab
  move focus between windows and items; arrow keys move within a group; Enter
  activates; Esc closes a dialog or backs out; the menu bar is reachable by a hot
  key (e.g. Alt or F10) with underlined mnemonics. A user on a mouseless 386SX
  laptop can drive the entire desktop. This matches the text UI, where nothing
  ever *requires* a mouse.

### 24.3.4 Program-manager launcher (reuses existing data)

CASTALIA DESK does not invent a new catalog. It **reads the same files the
text-mode launcher already uses**:

- `C:\CASTALIA\CFG\GAMES.INI` — the game catalog (title, path, executable,
  required memory profile, sound settings, per-game notes).
- `C:\CASTALIA\CFG\PROFILES.INI` — the memory-profile definitions that map to
  the `CONFIG.SYS` boot menu (CLEAN, XMS, EMS, CDROM, WIN3X, …).

Each `[game]` section in `GAMES.INI` becomes a program item (icon + label) inside
a program-group window. Groups can mirror the existing text-menu categories
(Action, Adventure, Strategy, Simulation, Tools, …). Because DESK and
`CASTALIA.EXE`/`LAUNCH.EXE` share the same source of truth, a game added once
appears in both. There is no second database to maintain and no way for the two
front ends to disagree.

Program groups for the built-in tools (`MEMPROF`, `SETSOUND`, `HWINFO`,
`CFGEDIT`, `GAMECFG`) are simply items whose target executable lives in
`C:\CASTALIA\BIN`.

### 24.3.5 Launching a game: exit cleanly to DOS first

This is the same discipline the text launcher uses, and it is non-negotiable.
CASTALIA DESK **must not** be in memory when a game runs. Sequence:

```
1. User double-clicks a game item in DESK.
2. DESK looks up the game in GAMES.INI: exe path, required profile, sound env.
3. If the game's required memory profile != the current boot profile, DESK
   writes a run-once request (via MEMPROF's mechanism) and asks the user to
   reboot into that profile — exactly as the text launcher does. DESK does not
   try to reconfigure memory live.
4. DESK writes a run-once batch (game dir change + SET BLASTER=... + the exe),
   restores the original text video mode, frees ALL its buffers, and EXITS to
   DOS.
5. COMMAND.COM runs the batch. The game now has the FULL conventional memory of
   the active profile (~615-631 KB depending on profile) with ZERO desktop
   overhead — identical to launching from the bare prompt.
6. On game exit, control returns to DOS. The user can relaunch DESK if desired.
```

The desktop is a *front porch*, not a *host*. It hands the machine over
completely and steps out of the room. There is no "run game inside the desktop,"
no DPMI host, no memory carved out for a GUI runtime while a game plays. The
"unload before launch" rule that governs `LAUNCH.EXE` governs DESK too, verbatim.

---

## 24.4 Video mode: text vs 320x200 vs 640x480

The desktop needs a mode that looks like a real GUI, uses the 16-color house
palette, and is *drawable at acceptable speed on a 386SX/16 with ISA VGA*. Three
realistic options were weighed:

| Mode                         | Pros                                | Cons on 386SX                              |
|------------------------------|-------------------------------------|--------------------------------------------|
| 80x25 / 80x50 **text**       | Fastest; trivially portable; safe   | Not a real desktop; coarse "windows"       |
| **320x200x256** (mode 13h)   | Linear framebuffer, easy to draw    | 256-color, off-palette; blocky; not 16-clr |
| **640x480x16** (mode 12h)    | Crisp real desktop; true 16-color   | VGA-only; **planar**, slow writes          |
| 640x350x16 (mode 10h, EGA)   | Real desktop; runs on EGA too       | Planar; slightly less room than 480        |

**Decision:**

- **Primary target: 640x480x16, VGA planar (BIOS mode 12h).** This is the only
  mode that gives a genuine 16-color GUI at desktop resolution and matches the
  brand palette exactly. It is planar (four bit-planes via the EGA/VGA sequencer
  and the write latches), which makes pixel writes more expensive than a linear
  framebuffer — precisely why the region model is **dirty-rectangle only** and
  why the visual style is flat, filled panels and single-pixel frames (cheap to
  draw) rather than gradients or fine detail (expensive). Solid rectangle fills
  and 1-bpp bitmap blits through the latch registers are the fast paths, and the
  whole look is designed around them.
- **EGA fallback: 640x350x16 (mode 10h)** for EGA-only machines, same drawing
  code, slightly shorter field.
- **Text fallback: 80x25/80x50** for the truly slow, the adapter-limited, or
  users who simply prefer it — the "EGA-ish" mockup in §24.7 is essentially this
  mode.
- **Rejected: 320x200x256 (mode 13h).** It is the classic "easy DOS graphics"
  mode and the fastest to draw, but it is a *256-color* mode. Forcing a 16-color
  desktop into it wastes the framebuffer, still looks blocky at 320-wide for text
  labels, and drifts off the 16-color identity. We would rather be crisp and
  planar-slow than blurry and linear-fast.

**Why the mode choice must not touch game memory or compatibility:** the video
mode question is *entirely internal to DESK's own runtime*. Whichever mode DESK
uses, it **saves the prior mode on entry and fully restores it on exit** before
handing off to a game (§24.3.5). A game never inherits DESK's video state,
never inherits a leftover graphics mode, and never shares memory with it. The
desktop's rendering budget is spent only while the desktop is on screen; the
instant a game launches, that budget is zero. There is no scenario in which
choosing 640x480 for the desktop costs a game a single byte or a single unit of
compatibility, because the desktop is gone before the game starts.

---

## 24.5 Why this is a FUTURE module, not core to 1.0

Four reasons, each tied back to the project's stated priorities
(see the **Vision** and **Performance** sections of this bible).

### Compatibility risk
CASTALIA DOS 1.0's contract is *"compatibility beats elegance."* A graphical
shell is a large new surface for things to go wrong: mouse driver quirks, VGA/EGA
mode edge cases, palette conflicts, adapters that lie about their capabilities.
None of that can be allowed to touch the core boot-and-run-a-game path. Keeping
the desktop as a removable module means a compatibility bug in DESK is, at worst,
a bug in an optional tool — never a reason a machine fails to boot or a game fails
to run. The core stays small, auditable, and trustworthy.

### 386SX performance budget
The flagship is the **386SX Edition** — a 16-bit-bus, no-FPU, often-16 MHz CPU
driving planar VGA. Every cycle is precious. A resident GUI would tax exactly the
machines we care most about. Even non-resident, a desktop must justify its draw
cost, which is why §24.3 and §24.4 are so austere (dirty rectangles, flat panels,
no compositor). Getting that right well is *more* engineering than 1.0 needs, and
1.0's performance story is "boot fast, free the most RAM, get out of the game's
way." A desktop is orthogonal to that story and must not dilute it.

### Development cost
1.0 already commits to a full text UI, a launcher, a memory-profile switcher,
sound config, diagnostics, a config editor, per-game config, and a rescue path —
all on a demanding toolchain and validated on real hardware. A windowing engine,
event loop, mouse UI, and graphics renderer are a substantial *additional* body
of work with its own testing matrix (multiple adapters, multiple mice, multiple
mode fallbacks). Spending that effort before 1.0 ships would delay the thing
that actually defines the product: a rock-solid DOS gaming environment.

### DOS games need DOS, not a desktop
This is the philosophical core. The reason to run CASTALIA DOS is to play DOS
games the way they were meant to run: real mode, full conventional memory, direct
hardware, nothing in the way. A desktop that inserted itself between the user and
the game would betray that. The correct role for any graphical layer here is to
**launch and then vanish** — which is exactly what §24.4 mandates. A desktop is a
nice front porch; the house is still DOS. 1.0 ships the house first.

---

## 24.6 Roadmap and exit criteria

CASTALIA DESK is a **post-1.0** effort. It does not appear in 1.0
"Tombatossals" or in the 1.1 "Montornés" maintenance line (1.1's headline new
tool is the `CASTFM.EXE` / `ALCAZAR.EXE` file manager, itself still text-mode).

| Milestone          | DESK status                                                    |
|--------------------|----------------------------------------------------------------|
| 1.0 Tombatossals   | Not present. Text UI only.                                      |
| 1.1 Montornés      | Not present. File manager lands (text-mode).                   |
| **1.x preview**    | **Optional technical preview** of CASTALIA DESK, opt-in only.  |
| **2.0**            | Candidate for a supported, still-optional release of DESK.     |

**Preview (1.x) exit criteria — all must hold before a preview ships:**

1. **Zero core impact.** With DESK uninstalled, the 1.x build is byte-for-byte
   behaviorally identical to the same build's core. Proven by test.
2. **Clean unload.** Launching a game from DESK leaves the game with the same
   free conventional memory (within measurement noise) as launching from the bare
   prompt, on all shipping memory profiles. Verified with `MEMPROF`/`MEM`.
3. **Runs on the reference 386SX/16.** The desktop is *usable* (pointer tracks,
   windows open, a game launches) on the reference 386SX with a few MB of RAM and
   ISA VGA — validated on real hardware, not just DOSBox-X/86Box.
4. **Single source of truth.** DESK reads `GAMES.INI`/`PROFILES.INI` and adds no
   second catalog; a game added in the text UI appears in DESK and vice versa.
5. **Keyboard-complete.** Every action is reachable without a mouse.
6. **Graceful fallback.** On EGA-only or user preference, DESK falls back to
   640x350 or text mode without a separate build.

**2.0 exit criteria (supported release)** add: broad adapter/mouse compatibility
matrix passed; documented and localized help pages in `C:\CASTALIA\HELP`; and a
formal review confirming the module still ships separately and stays off the
default boot path. If any of these slip, DESK stays a preview — it never blocks a
core release.

---

## 24.7 ASCII mockup (text / EGA-ish rendering)

A sketch of the program-manager surface at 80 columns, showing the desktop field,
menu bar, two program-group windows, and the pointer. In 640x480x16 the same
layout is drawn with pixel frames and a real arrow pointer; the palette is the
house scheme (blue field, gray/white panels, amber title bars, black-on-amber
selection).

```
+==============================================================================+
| File   Groups   Options   Game   Help                     CASTALIA DESK  =[]=|
+==============================================================================+
|                                                                              |
|   .----------------------.    .----------------------.                       |
|   | Action & Arcade  [_] |    | Strategy & Sim   [_] |                       |
|   +----------------------+    +----------------------+                       |
|   |  [##]   [##]   [##]  |    |  [##]   [##]   [##]  |                       |
|   | DOOM  WOLF3D  KEEN   |    | DUNE2 CIV     SC2K   |        \|/            |
|   |  [##]   [##]   [##]  |    |  [##]   [##]         |    ---->o<----        |
|   | RAPTR COMANDR XENON  |    | TRANSP RRTCOON       |        /|\            |
|   `----------------------'    `----------------------'                       |
|                                                                              |
|   .----------------------------.                                             |
|   | System Tools           [_] |  Profile: XMS  |  Sound: SB Pro             |
|   +----------------------------+                                             |
|   |  [MP]   [SS]   [HW]  [CF]  |  Free conv: 628 KB                          |
|   | MEMPRF SETSND HWINFO CFGE  |                                             |
|   `----------------------------'                                             |
|                                                                              |
+==============================================================================+
| Double-click a game to exit to DOS and launch it.   Tab: focus  Enter: run   |
+==============================================================================+
```

Notes on the mockup:

- The **`--->o<---` glyph** marks the mouse pointer position; in graphics mode
  this is a real arrow with save-under (§24.3.2).
- **Icons `[##]`** are 16-color bitmap tiles in graphics mode; here they are
  placeholder cells. Each maps to a `[game]` section in `GAMES.INI`.
- The **status strip** ("Profile: XMS | Sound: SB Pro | Free conv: 628 KB") reads
  live from the active memory profile and `SET BLASTER` — the same values
  `MEMPROF`/`SETSOUND` report — so the desktop and the text tools always agree.
- **Selection** (not shown highlighted here) is black-on-amber, matching the text
  UI's selected-bar style.
- The layout fits comfortably inside 80 columns and maps cleanly onto the
  640x480x16 grid, so the text fallback and the graphical mode are the *same
  design* at two fidelities.

---

*End of Section 24. CASTALIA DESK is deliberately fenced outside the 1.0 core.
The core ships DOS. The desktop, if and when it ships, launches from DOS and
returns to it — never in the way of a game.*
