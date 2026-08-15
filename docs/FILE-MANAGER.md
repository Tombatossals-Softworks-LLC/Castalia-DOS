# Section 12 — File Manager (CASTFM.EXE)

> **Implementation status:** a minimal single-pane prototype now exists at
> [`../src/castfm/CASTFM.C`](../src/castfm/CASTFM.C) (`wmake castfm`) — list,
> enter directories, change drive, free space, view text, run programs, and
> copy/move/delete/mkdir. It realises the "minimal first version" recommended
> below; the dual-pane and richer features remain future work. This section
> is the design; the code is the current subset.

> Part of the CASTALIA DOS technical bible. Ground truth: project
> `CONVENTIONS.md`. Target hardware: 386SX first. Toolchain: Open Watcom
> C/C++, 16-bit real-mode DOS, C89 style, large model, no dynamic allocation.

---

## 12.1 Purpose and Philosophy

CASTALIA DOS is a games-first, compatibility-first environment. Most users
spend their time in the **main menu** (`CASTALIA.EXE`) and the **launcher**
(`LAUNCH.EXE` / `GAMEVAULT.EXE`), not managing files. A file manager is a
convenience, not a load-bearing part of the boot or gaming path. That framing
drives every decision in this section.

The design goal is a **simple, light file manager in the *spirit* of Norton
Commander — but deliberately lighter than NC.** Where Norton Commander is a
dual-pane power tool with an internal editor, compression browsing, FTP-era
extensions, a scripting menu system and a large resident footprint, Castalia's
tool aims for the opposite end of the scale:

- **One job, done calmly.** List a directory, and let the user view, copy,
  move, delete, rename, make directories, change drives and launch programs.
  Nothing more in the first version.
- **Small and honest.** It must run comfortably in the **CLEAN** profile
  (~615 KB free conventional) and coexist with a game the user is about to
  launch. No large resident cache, no dynamic heap, no surprises.
- **Consistent with the rest of Castalia.** Same 80x25 text mode, same
  blue/gray/amber palette, same function-key idiom, same errorlevel/batch
  handoff for launching programs that the launcher already uses.
- **Never a substitute for DOS.** Real DOS already has `DIR`, `COPY`, `XCOPY`,
  `MOVE`, `DEL`, `REN`, `MD`, `RD` and `TYPE`. CASTFM is a friendlier front
  end for casual browsing, not a reimplementation of the shell.

The philosophy in one line: **a stone-and-steel `DIR` with arrow keys** — calm,
predictable, and small enough that you forget it is running.

---

## 12.2 Naming

The tool ships under one canonical name with two aliases (all three are the
same executable, or a `.EXE` plus small stub `.BAT`/`.EXE` copies — see note):

| Name           | Role            | Rationale                                    |
|----------------|-----------------|----------------------------------------------|
| `CASTFM.EXE`   | Primary         | Clear, self-describing: *Castalia File Mgr*. |
| `CFM.EXE`      | Short alias     | Fast to type at the prompt; muscle memory.   |
| `ALCAZAR.EXE`  | Branded alias   | *Alcázar* = Iberian fortress/citadel — fits  |
|                |                 | the Castilian fortress visual identity.      |

**Naming choice and reasoning.** `CASTFM.EXE` is the name used in menus, help
text and documentation because it is unambiguous and matches the naming pattern
of the other Castalia tools (`MEMPROF`, `SETSOUND`, `HWINFO`, `CFGEDIT`,
`GAMECFG`). `CFM.EXE` exists purely for ergonomics — three characters at the
`C:\>` prompt. `ALCAZAR.EXE` is the "personality" name: it reinforces the
fortress theme (the alcázar guards the keep the way the file manager guards your
files) and gives the product a memorable, brandable command. The 8.3 filename
limit is respected by all three (`ALCAZAR` is exactly 7 characters).

> **Implementation note on aliases.** To avoid shipping three ~30 KB binaries,
> the canonical build is `CASTFM.EXE`. `CFM.EXE` and `ALCAZAR.EXE` are tiny
> stub launchers (a few hundred bytes) that `exec` `CASTFM.EXE`, or — simpler
> and zero-code — they can be one-line batch files (`CFM.BAT`, `ALCAZAR.BAT`)
> installed on the PATH in `C:\CASTALIA\BIN`. The batch approach is preferred
> because it costs nothing to maintain and keeps a single real executable.

---

## 12.3 Feature Set — 1.0-if-built vs Deferred to 1.1

The table below is explicit about a **minimal first version** (the smallest
thing that is worth shipping at all) versus features that wait. "1.0-if-built"
means: *if* we build any file manager for 1.0 "Tombatossals", only these ship.
The recommendation in §12.4 is to **delay the full tool to 1.1 "Montornés"**,
but ship the minimal viewer/runner subset in 1.0 only if schedule allows.

| Feature                    | Minimal first version | Full 1.1 tool | Notes                                          |
|----------------------------|:---------------------:|:-------------:|------------------------------------------------|
| List files (findfirst/next)|        **Yes**        |     Yes       | Core; nothing works without it.                |
| Sort (name/ext/size/date)  |     Name only         |     Yes       | Minimal: name-ascending, dirs first.           |
| Attribute display          |        **Yes**        |     Yes       | R/H/S/A/D flags; read-only in minimal.         |
| View text file (pager)     |        **Yes**        |     Yes       | The "viewer" half of viewer/runner.            |
| Run executable (F-key/Enter)|       **Yes**        |     Yes       | The "runner" half; errorlevel handoff.         |
| Change drive               |        **Yes**        |     Yes       | Cheap; needed to be useful at all.             |
| Show free space            |        **Yes**        |     Yes       | One `INT 21h AH=36h` call; trivial.            |
| Change directory (Enter)   |        **Yes**        |     Yes       | Navigating into subdirs / `..`.                |
| Make directory (F7)        |         No            |     Yes       | Deferred: mutating op, needs input dialog.     |
| Copy (F5)                  |         No            |     Yes       | Deferred: read/write loop + progress + errors. |
| Move (F6)                  |         No            |     Yes       | Deferred: rename, or copy+delete cross-drive.  |
| Delete (F8)                |         No            |     Yes       | Deferred: destructive; needs confirm dialog.   |
| Rename                     |         No            |     Yes       | Deferred: input dialog + collision handling.   |
| Mouse support              |         No            |     Optional  | Nice-to-have; keyboard is the contract.        |
| Multi-select / tag files   |         No            |     Maybe     | Batch ops on tagged files; 1.1+ stretch.       |
| Dual-pane layout           |         No            |     Later     | See §12.5; single-pane is the 1.1 baseline.    |

**Why this split.** The minimal version is *read-only plus launch*: it can look
at the disk, read a text file, and start a program. It cannot modify the file
system. That makes it **safe to ship early** (no data-loss risk, no confirm
dialogs, no partial-write recovery) and it maps almost one-to-one onto the code
the launcher already needs (directory scan + program exec). Every deferred
feature is either **destructive** (delete, move, rename overwrite) or requires
a **text-input dialog** (mkdir, rename) or a **long-running operation with
progress and error recovery** (copy). Those are exactly the parts that cost
testing time and carry risk, so they belong in the dedicated 1.1 effort.

---

## 12.4 Recommendation — Delay to 1.1

**Recommendation: DELAY the file manager to 1.1 "Montornés."** Ship a **minimal
read-only viewer/runner in 1.0 "Tombatossals" only if time allows** after the
launcher and main menu are solid; otherwise, ship nothing and do it properly in
1.1.

### Justification

1. **Scope and risk.** The launcher (`LAUNCH.EXE`) and the main menu
   (`CASTALIA.EXE`) are the product. They are what a user sees at boot and what
   makes Castalia *Castalia*. Every engineering hour spent on a file manager in
   1.0 is an hour not spent hardening the boot menu, the memory-profile
   switching, sound configuration and game launching — the features that define
   1.0. A file manager is a convenience layer on top of a working system.

2. **DOS already ships the primitives.** `DIR`, `COPY`, `MOVE`, `DEL`, `REN`,
   `MD`, `RD`, `TYPE` all exist in the FreeDOS base in `C:\DOS`. A user who
   needs to manage files today can. The file manager improves *ergonomics*, not
   *capability* — a legitimate goal, but a 1.1 goal.

3. **NC-style clones already exist.** DOS Navigator, Volkov Commander and
   similar open/free Norton-Commander-style managers exist and run fine on a
   386SX. A user who wants a full dual-pane experience *right now* has options.
   Castalia's value-add is a **lighter, on-brand** tool integrated with the
   Castalia palette and launch handoff — worth doing well, not worth rushing.

4. **The destructive features are where the bugs live.** Copy across a full
   floppy, delete with a bad confirm, rename over an existing file, mkdir on a
   read-only medium — these are the operations that lose data if done wrong. A
   games distro cannot afford a file manager that eats a save game. Deferring
   them to a focused 1.1 milestone means they get the testing they demand.

5. **The minimal subset is nearly free.** Directory listing + program exec is
   code the launcher already contains. Wrapping it in a browsable pane and a
   text pager is a small, *safe*, read-only addition. If the 1.0 schedule has
   slack, shipping the viewer/runner is low-risk polish. If it does not, it is
   the first thing to cut with zero impact on the core product.

**Bottom line:** the full CASTFM is a 1.1 deliverable. In 1.0 it is a
*stretch goal in read-only form* and an easy, guilt-free cut.

---

## 12.5 UI Design

### 12.5.1 Single-pane vs Dual-pane

**Recommendation: single-pane for the 1.1 baseline.** Norton Commander's
signature is two side-by-side panes (source and destination). Castalia
deliberately does **not** copy this for the first real version:

- **Memory and complexity.** Two panes means two independent directory scans,
  two entry arrays, two sets of state (path, cursor, scroll, sort), and the
  logic to keep "active pane" straight. Single-pane halves the state and the
  screen-drawing code, and keeps the entry buffer small.
- **80x25 is tight.** A 40-column pane shows short filenames and little else.
  A full 78-column single pane can show name, size, date and attributes
  comfortably, which is more useful for casual browsing.
- **The common case is one directory.** Browse, view, launch. Copy/move needs a
  *destination*, but a single-pane tool solves that with a **prompt** ("Copy to:
  ____") — smaller and clearer than juggling a second pane.

Dual-pane is a **documented later option** (1.2+ or a `/2` switch), attractive
once the single-pane core is proven. It is not rejected forever; it is simply
not the baseline.

### 12.5.2 Screen Mockup (80x25, Castalia palette)

Palette per `CONVENTIONS.md`: **blue background (1)**, **light-gray/white
frames (7/15)**, **amber/yellow highlights (14)**, **selected bar
black-on-amber (attr `0xE0`)**. Attribute byte = `(bg << 4) | fg`.

```
+==============================================================================+
| CASTALIA DOS  -  File Manager (CASTFM)                     C:\CASTALIA\GAMES |  <- amber title, blue bg
+------------------------------------------------------------------------------+
| Name              Ext    Size   Date       Attr                              |  <- gray column header
+------------------------------------------------------------------------------+
| [..]                     <DIR>  1994-08-01                                    |
| DOOM                     <DIR>  1994-08-01                                    |
| KEEN                     <DIR>  1994-08-01                                    |
|>WOLF3D           EXE    123456  1992-05-05  ..A..                            |< <- selected: black on amber
| README           TXT      2048  1994-08-01  ..A..                            |
| SETUP            EXE     40960  1992-05-05  ..A..                            |
| GAME             DAT    655360  1994-08-01  R.A..                            |
| SAVED0           SAV      8192  1994-08-02  ..A..                            |
|                                                                              |
|      ( 32 items - use Up/Down/PgUp/PgDn - Enter opens or runs )             |
|                                                                              |
+------------------------------------------------------------------------------+
| Drive C:  [HDD]   Free: 61,341,696 bytes   Total: 209,715,200 bytes         |  <- gray status
+------------------------------------------------------------------------------+
| F3 View  F5 Copy  F6 Move  F7 MkDir  F8 Delete  F9 Menu  F10 Quit           |  <- amber keys, gray labels
+==============================================================================+
```

Colour roles in the mockup:

| Element              | Fg / Bg              | Attr byte | Notes                        |
|----------------------|----------------------|-----------|------------------------------|
| Screen background    | white on blue        | `0x1F`    | Base fill.                   |
| Frame / borders      | light gray on blue   | `0x17`    | Double-line box characters.  |
| Title text           | amber on blue        | `0x1E`    | Product + tool name.         |
| Column header        | white on blue        | `0x1F`    | Bright, above the divider.   |
| Directory entry      | light gray on blue   | `0x17`    | `<DIR>` shown for folders.   |
| File entry           | white on blue        | `0x1F`    | Normal files.                |
| Hidden/system entry  | dark gray on blue    | `0x18`    | De-emphasised.               |
| Selected row (cursor)| black on amber       | `0xE0`    | The moving highlight bar.    |
| Status line          | white on blue        | `0x1F`    | Drive, free/total space.     |
| Function-key labels  | white on blue        | `0x1F`    | Text after each key.         |
| Function-key numbers | black on amber       | `0xE0`    | `F3`, `F5`… stand out.       |

### 12.5.3 Function-Key Bar

The bottom row is fixed and always visible. Keys match the section brief:

| Key   | Action  | Availability                                              |
|-------|---------|-----------------------------------------------------------|
| `F3`  | View    | Open the highlighted text file in the pager (minimal: yes)|
| `F5`  | Copy    | Copy highlighted item to a prompted destination (1.1)     |
| `F6`  | Move    | Rename within drive, or copy+delete across drives (1.1)   |
| `F7`  | MkDir   | Prompt for a name, create a subdirectory (1.1)            |
| `F8`  | Delete  | Confirm, then delete file / remove empty directory (1.1)  |
| `F9`  | Menu    | Drop-down menu (sort, change drive, rename, refresh, help)|
| `F10` | Quit    | Return to `CASTALIA.EXE` or the DOS prompt                 |

In the **minimal 1.0 build**, the greyed-out keys (F5/F6/F7/F8) either do not
appear or display a calm "Not available in this version" message. F3, F9
(sort/drive/refresh only) and F10 are live, plus `Enter` to open/run.

### 12.5.4 Keyboard Navigation Model

Keyboard is the contract; every action is reachable without a mouse.

| Key(s)              | Behaviour                                                    |
|---------------------|-------------------------------------------------------------|
| `Up` / `Down`       | Move the highlight one entry.                                |
| `PgUp` / `PgDn`     | Move one screen page.                                        |
| `Home` / `End`      | Jump to first / last entry.                                  |
| `Enter`             | On `<DIR>`: change into it. On `.EXE/.COM/.BAT`: run it.     |
|                     | On other files: offer to View.                              |
| `Backspace`         | Go up one directory (same as selecting `[..]`).             |
| `Tab`               | Reserved (switches panes in a future dual-pane build).      |
| `F3`                | View highlighted file in the pager.                         |
| `F5`/`F6`/`F7`/`F8` | Copy / Move / MkDir / Delete (1.1).                          |
| `F9`                | Open the drop-down menu.                                     |
| `F10` / `Esc`       | Quit (Esc also closes dialogs/menus/the pager).             |
| `Ctrl`+`A`..`Z`     | Change drive: e.g. type a letter after the drive command.   |
| Letter keys         | Type-ahead: jump to next entry starting with that letter.   |

**Change drive** is offered two ways: via the F9 menu ("Change drive…") which
lists valid drives, and via a direct keystroke (a drive-letter prompt). The
minimal build uses the menu path only.

### 12.5.5 Mouse — Optional

Mouse support is **optional and off the critical path.** If a mouse driver
(`CTMOUSE`) is present, CASTFM may use INT 33h to:

- click a row to select it, double-click to open/run,
- click a function-key label to invoke it,
- drag the scroll region (stretch).

The tool must be **fully usable with the keyboard alone** and must run with no
mouse driver loaded. Mouse code is compiled in but gated on a successful INT 33h
AX=0000h reset (returns AX=FFFFh if a driver is installed). If absent, the tool
simply never polls the mouse. This keeps the CLEAN profile (no TSRs) fully
supported.

---

## 12.6 Technical Approach

C89 / Open Watcom, large model, `wcl -0 -bt=dos -ml -os`. **No dynamic
allocation** — all buffers are fixed-size and statically declared. All DOS
services below are via `INT 21h` using Watcom's `int86`/`intdos` or inline
`_asm`/`#pragma aux`.

### 12.6.1 Directory Listing — findfirst / findnext

Directory enumeration uses the classic DOS **Find First / Find Next** pair,
which write a 43-byte record into the **Disk Transfer Area (DTA)**.

| Call            | Service                | Inputs                                   |
|-----------------|------------------------|------------------------------------------|
| Set DTA         | `INT 21h AH=1Ah`       | `DS:DX` -> our 43-byte DTA buffer         |
| Get DTA         | `INT 21h AH=2Fh`       | returns `ES:BX` (save/restore around ops)|
| Find First File | `INT 21h AH=4Eh`       | `DS:DX` -> ASCIIZ mask, `CX` = attr mask  |
| Find Next File  | `INT 21h AH=4Fh`       | uses the same DTA from Find First         |

The scan pattern is `*.*` in the current directory. The attribute mask in `CX`
requests directories in addition to normal files:

```
CX = 0x10 (directory) | 0x02 (hidden) | 0x04 (system) | 0x01 (read-only)
```

DTA record layout consumed after each successful call:

| Offset (hex) | Size | Field                                              |
|--------------|------|----------------------------------------------------|
| `00`         | 21   | Reserved (DOS search state for Find Next)          |
| `15`         | 1    | Attribute byte                                     |
| `16`         | 2    | File time (packed)                                 |
| `18`         | 2    | File date (packed: yyyyyyym mmmddddd from 1980)    |
| `1A`         | 4    | File size (dword, bytes)                            |
| `1E`         | 13   | Filename, ASCIIZ, 8.3 with dot (e.g. `WOLF3D.EXE`) |

Carry flag set (or AX=18 "no more files") ends the enumeration.

### 12.6.2 Fixed-Size Entry Array and the 512-Entry Cap

No heap. The directory is read into a **statically allocated array** with a
hard cap:

```c
#define FM_MAX_ENTRIES  512      /* per-directory cap */
#define FM_NAME_LEN     13       /* 8.3 + dot + NUL   */

typedef struct {
    char   name[FM_NAME_LEN];    /* "WOLF3D.EXE\0"           */
    unsigned long size;          /* bytes (0 for <DIR>)      */
    unsigned int  date;          /* packed DOS date          */
    unsigned int  time;          /* packed DOS time          */
    unsigned char attr;          /* R/H/S/D/A bits           */
} FM_ENTRY;

static FM_ENTRY fm_list[FM_MAX_ENTRIES];   /* ~512 * 24 B ~= 12 KB */
static int      fm_count;                  /* entries actually read */
static int      fm_truncated;              /* 1 if dir overflowed   */
```

At ~24 bytes per entry, 512 entries cost ~12 KB of static data — trivial for a
386SX and safe under the large model. The cap is documented and **overflow is
handled gracefully, never by crashing**:

- Enumeration stops accepting entries once `fm_count == FM_MAX_ENTRIES`, but
  Find Next continues to be drained so the DTA/search closes cleanly.
- `fm_truncated` is set, and the status line shows a calm warning, e.g.
  `Directory too large: showing first 512 of N items`.
- The user can still operate on the visible entries. Since real game
  directories rarely exceed a few hundred files, the cap is generous in
  practice; a directory with more than 512 entries is a signal to use `DIR`
  with wildcards or the DOS shell for that specific case.

> **Why a fixed cap instead of `malloc`?** Determinism. On a 386SX with the
> CLEAN profile there is no room for heap fragmentation surprises before
> launching a game. A fixed array has a known worst-case footprint that can be
> reasoned about at build time — exactly the Castalia toolchain philosophy.

### 12.6.3 Attribute Display

The `attr` byte is rendered as a five-character flag string, dashes for unset:

| Bit    | Value  | Flag char | Meaning       |
|--------|--------|-----------|---------------|
| 0      | `0x01` | `R`       | Read-only     |
| 1      | `0x02` | `H`       | Hidden        |
| 2      | `0x04` | `S`       | System        |
| 5      | `0x20` | `A`       | Archive       |
| 4      | `0x10` | `D`       | Directory     |

Rendered as `RHSAD` positions, e.g. a normal archived file is `..A..`, a
read-only archived file is `R.A..`, a subdirectory is `...D.` (shown as
`<DIR>` in the size column as well). Volume-label entries (`0x08`) are skipped
during enumeration — they are not files.

### 12.6.4 Sorting

Sorting is done in place on `fm_list` after the scan, with a simple
comparison-based sort (`qsort` from the C library is acceptable; a small
insertion/shell sort avoids even that dependency and is fine for <=512 items).
**Directories always sort before files**, `[..]` always first. Sort keys:

| Key    | Order                         | Availability      |
|--------|-------------------------------|-------------------|
| Name   | Alphabetical, dirs first      | Minimal + 1.1     |
| Ext    | By extension, then name       | 1.1               |
| Size   | Ascending / descending        | 1.1               |
| Date   | Oldest / newest first         | 1.1               |

The comparison honours the "dirs first, `[..]` always top" rule before applying
the chosen key so navigation stays predictable.

### 12.6.5 Drive Enumeration and Change Drive

| Task              | Service           | Detail                                        |
|-------------------|-------------------|-----------------------------------------------|
| Current drive     | `INT 21h AH=19h`  | Returns `AL` (0=A, 1=B, 2=C…).                |
| Select drive      | `INT 21h AH=0Eh`  | `DL` = drive; returns `AL` = # logical drives.|
| Validate a drive  | `INT 21h AH=36h`  | Free-space call returns `AX=FFFFh` if invalid.|

Drive enumeration walks A: through the last logical drive reported by
`AH=0Eh`, calling the free-space service on each and listing only those that do
**not** return the `FFFFh` invalid marker. This avoids the notorious "Abort,
Retry, Fail" critical-error prompt: CASTFM installs no INT 24h handler beyond a
minimal one that fails silently, and prefers the non-intrusive `AH=36h` probe
to detect a not-ready floppy before touching it.

### 12.6.6 Free Space — INT 21h AH=36h

```c
/* Returns free bytes on 'drive' (1=A,2=B,3=C...), 0 on error. */
static unsigned long fm_free_bytes(unsigned char drive)
{
    union REGS r;
    unsigned long spc, bps, freeclus;
    r.h.ah = 0x36;
    r.h.dl = drive;              /* 0 = default, 1 = A, 2 = B, 3 = C */
    int86(0x21, &r, &r);
    if (r.x.ax == 0xFFFF)        /* invalid drive */
        return 0UL;
    spc      = r.x.ax;           /* sectors per cluster       */
    freeclus = r.x.bx;           /* free clusters             */
    bps      = r.x.cx;           /* bytes per sector          */
    /* total clusters = r.x.dx (used for Total: display)      */
    return spc * bps * freeclus; /* free bytes                */
}
```

Total capacity uses the same call: `total_bytes = spc * bps * DX`. Both are
formatted with thousands separators for the status line. On FAT16 volumes near
2 GB the product can exceed 32 bits; CASTFM clamps/annotates rather than
overflowing (a 386SX-era CF card or HDD is far below that, so this is an
edge-case guard, not a common path).

### 12.6.7 Text Viewer with Paging

`F3` opens the highlighted file in a **read-only pager** (the "viewer" half of
the minimal viewer/runner). Design:

- Open with `INT 21h AH=3Dh` (mode 0 = read-only), read in fixed 4–8 KB chunks
  with `AH=3Fh`, close with `AH=3Eh`. **No whole-file buffering** — the file
  may be larger than free RAM.
- A fixed line-offset table (e.g. `unsigned long line_ofs[FM_MAX_VIEW_LINES]`,
  cap ~2000) records the byte offset of each line's start as the user scrolls,
  so `PgUp`/`Home` can seek backward with `AH=42h` (LSEEK) instead of holding
  the file in memory. Past the cap, backward paging re-seeks from the top.
- Renders 23 text rows + title + key bar. Non-printable bytes shown as `.`
  (a light "binary-ish" guard); the viewer is intended for text but must not
  corrupt the screen if pointed at a binary.
- Keys: `Up`/`Down`/`PgUp`/`PgDn`/`Home`/`End` to scroll, `Esc`/`F10`/`Q` to
  return. Optional `F` to toggle a simple wrap vs truncate at column 78.

```
+==============================================================================+
| View: C:\CASTALIA\GAMES\README.TXT            line 1 of 240      [ text ]     |
+------------------------------------------------------------------------------+
| WOLFENSTEIN 3-D  -  installation notes                                        |
| ------------------------------------------                                    |
| Run SETUP.EXE first to choose your sound card, then start with WOLF3D.EXE.    |
| ...                                                                           |
+------------------------------------------------------------------------------+
| Up/Dn PgUp/PgDn scroll   Home/End jump   Esc back                            |
+==============================================================================+
```

### 12.6.8 Launching Executables — Errorlevel / Batch Handoff

CASTFM **reuses the launcher's handoff pattern** rather than inventing its own.
There are two mechanisms, matching how `LAUNCH.EXE` behaves:

1. **Direct child exec (`INT 21h AH=4Bh`, EXEC).** For simple cases CASTFM can
   spawn the program as a child with function `4B00h`, building a command tail
   and environment, then regain control when the child exits. This keeps CASTFM
   resident (~30 KB) *plus* DOS overhead during the game — acceptable for small
   tools, **not ideal before a memory-hungry game**.

2. **Errorlevel + batch handoff (preferred for games).** The Castalia-standard
   pattern: CASTFM writes the chosen program's path/command into a well-known
   handoff file (e.g. `C:\CASTALIA\CFG\RUN.BAT` or a line in a run-request
   file), sets a specific **errorlevel** via `INT 21h AH=4Ch` (exit with code),
   and **terminates**. The wrapping batch file (the same one that launched
   CASTFM/the menu) inspects `ERRORLEVEL` and `CALL`s the generated batch. This
   frees **all** of CASTFM's memory before the game runs — the whole point on a
   386SX with ~615–631 KB conventional.

The exit-code contract mirrors the launcher and the memory-profile switcher so
the outer `AUTOEXEC`/menu batch can route consistently:

| Errorlevel | Meaning                                                        |
|------------|----------------------------------------------------------------|
| `0`        | Normal quit — return to `CASTALIA.EXE` / prompt.               |
| `10`       | Run the handoff batch (`RUN.BAT`) then return to the menu.     |
| `20`       | Run the handoff batch and **exit to DOS** (do not re-enter).   |
| `>=100`    | Reserved for errors, mirroring the launcher's convention.      |

Because the handoff file and errorlevel scheme are **shared** across
`LAUNCH.EXE`, `MEMPROF.EXE` and `CASTFM.EXE`, one outer batch dispatcher serves
all three. This is why launching from the file manager feels identical to
launching from the game vault — it *is* the same mechanism.

### 12.6.9 Mutating Operations (1.1) — Services Used

For completeness, the deferred write operations map to these services:

| Operation      | Service(s)                                    | Notes                          |
|----------------|-----------------------------------------------|--------------------------------|
| Make directory | `INT 21h AH=39h` (MKDIR)                       | F7; prompt for name first.     |
| Remove dir     | `INT 21h AH=3Ah` (RMDIR)                        | Empty dirs only; confirm.      |
| Delete file    | `INT 21h AH=41h` (UNLINK)                        | F8; confirm dialog.            |
| Rename / move  | `INT 21h AH=56h` (RENAME)                         | Same drive only.               |
| Copy           | `3Dh` open, `3Ch` create, `3Fh`/`40h` R/W loop, `3Eh` close | F5; 8–16 KB buffer, progress.  |
| Cross-drive move| Copy (as above) then `41h` delete source        | F6 when drives differ.         |
| Attributes     | `INT 21h AH=43h` (get/set)                         | Clear R/O before delete, etc.  |

All mutating operations in 1.1 are guarded by a **confirm dialog** and check the
carry flag / AX return for failure, surfacing a calm error box ("Cannot copy:
disk full") rather than the raw DOS critical-error prompt. The copy buffer is a
fixed 8–16 KB static array — again, no heap.

---

## 12.7 Data / Interaction Flow and Main-Loop Pseudocode

### 12.7.1 Interaction Flow

```
        start CASTFM.EXE
              |
              v
   [ save current DTA ] ---> [ set our DTA ]
              |
              v
   [ get current drive + dir ] --> path state
              |
              v
   +----> [ scan dir: FindFirst/FindNext into fm_list, cap 512 ] 
   |          |
   |          v
   |     [ sort: dirs first, name asc ]
   |          |
   |          v
   |     [ draw screen: title, list, status(free space), key bar ]
   |          |
   |          v
   |     [ wait for key / (optional) mouse event ]
   |          |
   |   +------+----------------------------------------------+
   |   |      |            |            |          |          |
   |  move   Enter        F3          F9         F10       F5..F8
   |  cursor  |            |           |           |         (1.1)
   |   |      v            v           v           v          |
   |   |  dir? chdir   open pager   menu(sort/   set errlvl   confirm
   |   |  exe? handoff             drive/help)   + exit       + op
   |   |  else? view       |           |           |          |
   |   |      |            |           |           |          |
   +---+------+------------+-----------+           |          |
   |                                               |          |
   |   (chdir / drive change / refresh) -----------+          |
   |         re-scan the new directory                        |
   +----------------------------------------------------------+
                                                   |
                                                   v
                              [ restore caller's DTA ]
                                                   |
                                                   v
                              [ INT 21h AH=4Ch exit(errorlevel) ]
```

Key points: exactly **one** directory scan lives in memory at a time
(single-pane), the DTA is saved on entry and restored on exit so the calling
shell is undisturbed, and every path that changes the directory or drive routes
back through the single `scan -> sort -> draw` sequence.

### 12.7.2 Main-Loop Pseudocode

```c
int main(void)
{
    int  sel = 0;          /* index of highlighted entry      */
    int  top = 0;          /* index of first visible row      */
    int  running = 1;
    int  exit_code = 0;    /* errorlevel handed back to batch */

    fm_save_caller_dta();
    fm_set_our_dta();
    fm_get_cwd(path);                 /* AH=47h + current drive */

    fm_scan(path);                    /* FindFirst/FindNext -> fm_list */
    fm_sort_name_dirs_first();

    while (running) {
        fm_draw_screen(path, sel, top);   /* list + status + keys */
        if (fm_truncated)
            fm_status_warn("Showing first 512 items");

        key = fm_read_key();              /* BIOS INT 16h; poll mouse if present */

        switch (key) {
        case K_UP:    if (sel > 0) sel--;            break;
        case K_DOWN:  if (sel < fm_count-1) sel++;   break;
        case K_PGUP:  sel -= PAGE; if (sel<0) sel=0; break;
        case K_PGDN:  sel += PAGE;
                      if (sel>=fm_count) sel=fm_count-1; break;
        case K_HOME:  sel = 0;                       break;
        case K_END:   sel = fm_count-1;              break;

        case K_ENTER:
            if (fm_list[sel].attr & 0x10) {          /* directory */
                fm_chdir(fm_list[sel].name);         /* AH=3Bh    */
                fm_get_cwd(path);
                fm_scan(path); fm_sort_name_dirs_first();
                sel = top = 0;
            } else if (fm_is_program(fm_list[sel].name)) {
                fm_write_handoff(path, fm_list[sel].name);
                exit_code = 10;                       /* run + return */
                running = 0;
            } else {
                fm_view(path, fm_list[sel].name);     /* pager */
            }
            break;

        case K_BACKSPACE:
            fm_chdir("..");
            fm_get_cwd(path);
            fm_scan(path); fm_sort_name_dirs_first();
            sel = top = 0;
            break;

        case K_F3:                                    /* View */
            if (!(fm_list[sel].attr & 0x10))
                fm_view(path, fm_list[sel].name);
            break;

        case K_F5: case K_F6: case K_F7: case K_F8:   /* 1.1 ops */
            if (FM_MINIMAL_BUILD)
                fm_status_warn("Not available in this version");
            else
                fm_do_mutating_op(key, path, &sel);   /* re-scans on success */
            break;

        case K_F9:                                    /* Menu */
            switch (fm_menu()) {                       /* returns choice */
            case M_SORT:   fm_cycle_sort();  fm_resort();       break;
            case M_DRIVE:  if (fm_pick_drive(&drive)) {
                               fm_select_drive(drive); /* AH=0Eh */
                               fm_get_cwd(path);
                               fm_scan(path); fm_sort_name_dirs_first();
                               sel = top = 0;
                           }                                    break;
            case M_REFRESH:fm_scan(path); fm_resort(); sel=0;   break;
            case M_HELP:   fm_show_help();                      break;
            case M_QUIT:   exit_code = 0; running = 0;          break;
            }
            break;

        case K_F10:
        case K_ESC:
            exit_code = 0;                             /* clean quit */
            running = 0;
            break;
        }

        /* keep the highlight on screen */
        if (sel < top)            top = sel;
        if (sel >= top + PAGE)    top = sel - PAGE + 1;
    }

    fm_restore_caller_dta();
    return exit_code;          /* AH=4Ch via C runtime exit() */
}
```

### 12.7.3 Notes on the Loop

- **Single scan in memory.** Every navigation branch (`Enter` into a dir,
  `Backspace`, drive change, refresh) funnels back through `fm_scan` +
  `fm_sort`, so there is never more than one directory buffered — matching the
  single-pane, no-heap design.
- **Read-only-first.** In `FM_MINIMAL_BUILD`, the entire `K_F5..K_F8` branch is
  a polite refusal, so the 1.0 subset compiles from the same source with one
  `#define` and carries zero destructive code.
- **Program detection.** `fm_is_program` matches `.EXE`, `.COM`, `.BAT` (case-
  insensitive on the stored 8.3 name). `Enter` on those hands off; on a
  directory it navigates; on anything else it views. F3 always views.
- **Handoff, not resident.** The chosen path is written to the shared handoff
  file and CASTFM exits with errorlevel `10` (run then return) or `20` (run then
  exit) so the outer batch — the *same* dispatcher used by `LAUNCH.EXE` and
  `MEMPROF.EXE` — frees all of CASTFM's memory before the game starts.
- **Politeness on exit.** The caller's DTA is restored and the screen can be
  cleared/handed back to the menu so the transition is seamless.

---

## 12.8 Summary

- CASTFM is a **light, single-pane, on-brand** file manager, lighter than
  Norton Commander. Names: `CASTFM.EXE` (primary), `CFM.EXE` (short),
  `ALCAZAR.EXE` (branded, fortress theme).
- **Recommendation: delay the full tool to 1.1 "Montornés";** ship only a
  **minimal read-only viewer/runner** in 1.0 "Tombatossals" if schedule allows
  — the first, guilt-free cut, since DOS already has `DIR`/`COPY`/etc.,
  NC-clones exist, and the launcher + main menu are higher priority.
- **Technical spine:** `INT 21h` findfirst/findnext (`4Eh`/`4Fh`) into a fixed
  512-entry array (documented cap, graceful overflow), attribute flags, in-place
  sort, drive enumeration via `0Eh`/`36h`, free space via `AH=36h`, a seek-based
  text pager, and the **shared errorlevel/batch handoff** so launching a game
  frees all of the tool's memory first — C89 / Open Watcom, no dynamic
  allocation, honest about 386SX limits.
