# 23. Product Personality and Branding

> Part of the CASTALIA DOS technical bible.
> © 2026 The Castalia DOS Project. Documentation licensed CC BY 4.0.
> Branding marks and artwork are proprietary to the project; the ASCII/text
> logos in this section are original work and are safe to ship.

This section defines who CASTALIA DOS *is* — its personality, its voice, its
16-color face, and the exact words it says at the boot screen, in Setup, in
Help, and when something goes wrong. Everything here serves one goal: a serious,
dignified, genuinely useful DOS-compatible environment for real retro hardware.

---

## 23.1 Brand essence

**One line.** *A serious, elegant DOS-compatible environment for real machines
— a Mediterranean fortress for your 386.*

**What Castalia is.**

- A practical, beautiful, game-focused **DOS-compatible operating environment**
  built on **FreeDOS** and other open/free components, with original Castalia
  tools, menus, installer, and branding on top.
- Built for **real hardware first** — 386SX, 386DX, 486, early Pentium — with
  limited RAM, VGA, a serial or PS/2 mouse, IDE/CompactFlash, and AdLib/Sound
  Blaster audio. It runs beautifully in DOSBox-X and 86Box too, but the target
  is metal.
- **Compatibility beats elegance.** When a choice must be made between a prettier
  design and a game that actually runs, the game wins. The elegance is in
  service of the machine, never the other way around.

**What Castalia is not.**

- Not a toy, not a joke skin, not a "retro aesthetic" pasted over a modern OS.
- Not a Microsoft product and not a clone of one. It ships no Microsoft code,
  binaries, manual text, or branding (see §23.9).
- Not childish. No mascots with googly eyes, no comic fonts, no exclamation-mark
  confetti, no cutesy error faces.

### Personality, in five words

**Dignified · Precise · Warm · Sturdy · Unhurried.**

Think of the feeling of a well-made IBM-era manual, a Castilian keep in warm
afternoon light, and a technician who has fixed this exact problem a hundred
times and is not the least bit worried. Castalia is the calm expert in the room.

### The fortress metaphor (used consistently, never overplayed)

CASTALIA takes its identity from the **stone fortresses of Valencia and
Castellón** — Peñíscola on its sea-rock, Morella on its hill, the keep of
Sagunto. The metaphor is functional, not decorative:

| Fortress idea        | What it means in the product                          |
|----------------------|-------------------------------------------------------|
| The keep (*torreón*) | The core: kernel, shell, the Castalia main menu.      |
| The walls            | Compatibility and memory profiles that protect games. |
| The gate             | The boot menu — you choose how you enter.             |
| The armory           | The tools: MEMPROF, SETSOUND, HWINFO, CFGEDIT.        |
| The garrison         | Safe Mode and the rescue tools that hold the line.    |

Use the metaphor for *structure and reassurance*. Do not narrate it. The user
should feel solidity, not read a fantasy novel.

### Material and color language

Mediterranean **stone** (warm gray), tempered **steel** (cool gray/white),
**amber** lamplight (the highlight color), and deep Mediterranean **blue** (the
ground everything sits on). These four materials map directly onto the 16-color
palette in §23.3.

---

## 23.2 Voice and tone

Castalia speaks in **plain, calm, complete sentences**. It is precise like a
service manual and warm like a good teacher. It never blames the user, never
shouts, never pads. It respects that the person on the other side is running a
30-year-old machine on purpose and knows what they are doing.

**Principles.**

1. **Calm before clever.** Clarity first. A dignified sentence beats a witty one.
2. **Never blame the user.** Describe the situation and the next step, not fault.
3. **Precise, not verbose.** Say the thing. Stop. Screen space is 80×25.
4. **Warm, not chummy.** Courteous, not casual. No slang, no memes, no emoji.
5. **Confident, not boastful.** State facts plainly; let them carry the weight.
6. **Honest about limits.** A 386SX has real constraints. We say so, kindly.
7. **Consistent terminology.** "profile", "conventional memory", "keep", "gate"
   mean the same thing everywhere.

### Do / Don't

| Do                                                   | Don't                                              |
|------------------------------------------------------|----------------------------------------------------|
| "No sound card was detected. Sound is set to PC Speaker." | "ERROR!!! Sound card missing!!!"              |
| "Select a memory profile, then press Enter."         | "Just pick one lol, XMS is probably fine."         |
| "This drive is not ready. Insert a disk and retry."  | "You forgot to put in a disk again."               |
| "Windows 3.x mode needs the EMS profile."            | "Ugh, you can't run Windows like that."            |
| "Saved. Your previous settings are in C:\CASTALIA\BACKUP." | "Done!!! (I hope that worked )"              |
| "Free conventional memory: 631 KB."                  | "MASSIVE 631K free — you're gonna love this!"      |
| "Press Esc to return to the keep."                   | "Hit escape to bail out."                          |
| "This game expects EMS. Switching profiles may help."| "That game is broken, not my problem."             |

### Register cheat-sheet

- **Address the machine's state, not the person.** "The port is in use," not
  "You picked the wrong port."
- **Offer the next action.** Every message that reports a problem names a way
  forward.
- **Use active, present tense.** "Castalia writes a backup before changing
  CONFIG.SYS." Not "A backup will have been created."
- **Numbers are facts, not hype.** Report KB, IRQs, and ports plainly.
- **One idea per line.** The screen is narrow; wrapping is ugly.

### Words we prefer / avoid

| Prefer                          | Avoid                                   |
|---------------------------------|-----------------------------------------|
| profile, memory profile         | mode-thingy, config-blob                |
| conventional / upper / XMS / EMS| "normal RAM", "high memory magic"       |
| the keep, the main menu         | the dashboard, the home screen          |
| detected / not detected         | found it! / nope, gone                  |
| return, cancel, retry           | bail, nuke, yeet                        |
| please / thank you (sparingly)  | please please, pretty please           |

---

## 23.3 The 16-color VGA text palette

Castalia uses the standard **VGA 80×25 text mode, 16 colors**. The palette below
is the default IBM/VGA CGA-compatible set. Foreground can use all 16 colors;
background uses colors 0–7 (the high bit of the background nibble is the blink
attribute, which Castalia leaves **off** so panels can use bright backgrounds
and nothing flashes).

### The 16 colors

| # | Name          | Castalia material | Typical use                              |
|---|---------------|-------------------|------------------------------------------|
| 0 | Black         | shadow            | text on bright bars; frame shadow        |
| 1 | Blue          | Mediterranean sea | **the desktop background**               |
| 2 | Green         | —                 | success/OK text (used sparingly)         |
| 3 | Cyan          | —                 | secondary info, hints                    |
| 4 | Red           | —                 | critical/danger text (sparingly)         |
| 5 | Magenta       | —                 | rarely used; reserved                    |
| 6 | Brown         | dark stone        | inactive amber; muted labels             |
| 7 | Light Gray    | steel             | **panel/window background**              |
| 8 | Dark Gray     | stone shadow      | disabled text; dimmed items              |
| 9 | Light Blue    | —                 | links/keys within panels                 |
|10 | Light Green   | —                 | live "ready" indicators                  |
|11 | Light Cyan    | —                 | active hints, tips                        |
|12 | Light Red     | —                 | error text on gray panels                |
|13 | Light Magenta | —                 | reserved                                 |
|14 | Yellow/Amber  | **amber lamp**    | **highlights, hotkeys, titles, focus**   |
|15 | White         | bright steel      | panel body text; frames; headings        |

Amber (14) is the signature accent. It is the lamplight in the keep: titles,
active hotkey letters, the focused field, and the selection bar all use it. Blue
(1) is the ground. Gray (7) and white (15) are the fortress walls. Everything
else is used with restraint.

### How an attribute byte works

Each character cell has one **attribute byte**: the low nibble is the foreground
color (0–15), the high nibble is the background (0–7 with blink off). The numeric
value is `background * 16 + foreground`.

```
 attribute = (bg << 4) | fg          ; blink bit (0x80) left clear
 example  : white on blue = (1<<4)|15 = 0x1F = 31
```

### Castalia attribute reference table

These are the canonical foreground/background pairs the UI uses. Every Castalia
tool draws from this table so the whole system looks like one product.

| Element                     | FG (name)     | BG (name)     | Attr (hex) | Attr (dec) |
|-----------------------------|---------------|---------------|------------|------------|
| Desktop background          | Light Gray 7  | Blue 1        | `0x17`     | 23         |
| Desktop title text          | Yellow 14     | Blue 1        | `0x1E`     | 30         |
| Panel / window body         | Black 0       | Light Gray 7  | `0x70`     | 112        |
| Panel body text (bright)    | White 15      | Light Gray 7  | `0x7F`     | 127        |
| Panel title bar             | White 15      | Blue 1        | `0x1F`     | 31         |
| Panel frame (single/double) | White 15      | Light Gray 7  | `0x7F`     | 127        |
| Hotkey letter (in label)    | Light Red 12  | Light Gray 7  | `0x7C`     | 124        |
| Selection bar (focused)     | Black 0       | Yellow 14     | `0xE0`     | 224        |
| Selection bar (unfocused)   | Black 0       | Dark Gray 8   | `0x80`     | 128        |
| Menu item (normal)          | Black 0       | Light Gray 7  | `0x70`     | 112        |
| Menu item (disabled)        | Dark Gray 8   | Light Gray 7  | `0x78`     | 120        |
| Status/help line (bottom)   | Black 0       | Cyan 3        | `0x30`     | 48         |
| Status line hotkey          | White 15      | Cyan 3        | `0x3F`     | 63         |
| Field (input, focused)      | White 15      | Blue 1        | `0x1F`     | 31         |
| Field (input, unfocused)    | Black 0       | Dark Gray 8   | `0x80`     | 128        |
| "Ready/OK" indicator        | Light Green 10| Light Gray 7  | `0x7A`     | 122        |
| Warning text                | Brown 6       | Light Gray 7  | `0x76`     | 118        |
| Error text (in panel)       | Light Red 12  | Light Gray 7  | `0x7C`     | 124        |
| Critical banner             | White 15      | Red 4         | `0x4F`     | 79         |
| Boot banner rule/border     | Light Gray 7  | Blue 1        | `0x17`     | 23         |
| Boot banner accent          | Yellow 14     | Blue 1        | `0x1E`     | 30         |

Rules that keep it coherent:

- **The desktop is always blue (1).** Everything floats on the sea.
- **Panels are always gray (7)** with black or white text — the stone walls.
- **Focus is always amber (14).** If a user is looking for "where am I," it is
  the amber bar. There is exactly one amber selection bar visible at a time.
- **Red is rationed.** Red (4/12) appears only for genuine danger or errors, so
  it keeps its meaning.
- **No blinking.** The blink attribute stays off; bright backgrounds are used
  instead. Nothing in Castalia flashes at you.

---

## 23.4 Boot screen copy

Shown after POST, before the CONFIG.SYS boot menu. Dignified, quiet, exactly
80 columns wide. Drawn as white/amber on blue (attributes `0x1F` / `0x1E` on the
`0x17` field). The banner is centered in an 80-column frame.

```
================================================================================

                    C A S T A L I A   D O S   386SX Edition

              A DOS-compatible operating environment for real machines
                     Built on FreeDOS and free/open components

                       (C) 2026 The Castalia DOS Project

--------------------------------------------------------------------------------
   Preparing the keep. One moment, please.
================================================================================
```

CP437 box-drawing variant (same content, drawn with line characters that render
in text mode; 80 columns including the border):

```
╔══════════════════════════════════════════════════════════════════════════════╗
║                                                                              ║
║                 C A S T A L I A   D O S   -   386SX Edition                  ║
║                                                                              ║
║           A DOS-compatible operating environment for real machines           ║
║                  Built on FreeDOS and free/open components                   ║
║                                                                              ║
║                      (C) 2026 The Castalia DOS Project                       ║
║                                                                              ║
╚══════════════════════════════════════════════════════════════════════════════╝
        Preparing the keep.  Choose how you enter at the gate below.
```

Notes:
- The version line names the **edition** (386SX Edition). The codename appears on
  the About screen (§23.5), not on the busy boot banner.
- No progress bar theatrics. One quiet line of reassurance is enough.
- If the machine is slow, the line "One moment, please." earns its keep — it
  tells the user the pause is expected, not a hang.
- **No FreeDOS/Open Watcom sign-on.** FreeCOM's "FreeCom version … - WATCOMC
  …" banner is blanked at build time by `scripts/rebrand-dos.py`, and the
  FreeDOS kernel is **rebuilt from source** with `signon()` rewritten to a
  single `CASTALIA DOS 386SX Edition` line (`scripts/build-kernel.sh` +
  `scripts/patch-kernel-src.py`), so neither the kernel nor the shell prints
  "FreeDOS" or "WATCOMC" at boot. FreeDOS's copyright and GPL notice are
  retained in the shipped (patched) source, `LICENSES/`, and the About screen —
  see [`LICENSE-STRATEGY.md`](LICENSE-STRATEGY.md) §2.3.2a. Right after the
  kernel line, `BANNER.EXE` raises the keep and lights the **CASTALIA DOS**
  wordmark (the block-letter mark from §23.9), the animated companion to this
  quiet copy.
- **Credits at the gate.** Under the lit wordmark the animated `BANNER.EXE`
  shows the edition/version line, the studio line *a Tombatossals Softworks
  product* (light gray), and a quiet one-line creators credit — *Dave Abellan
  · Claudio di Castello* — in dim gray, above the
  boot profile and the *the fortress holds* sign-off. The full authorship
  block still lives on the About screen (§23.5); this is its understated boot
  form, so the machine credits its makers the moment it wakes.

---

## 23.5 About screen copy

Reached from the Castalia main menu (Help → About) or by running `CASTALIA /VER`.
Drawn in a centered gray panel (`0x70`) with a white title bar (`0x1F`).

```
┌─ About CASTALIA DOS ─────────────────────────────────────────────┐
│                                                                  │
│   CASTALIA DOS 386SX Edition  -  1.0 "Tombatossals"              │
│                                                                  │
│   A DOS-compatible operating environment for real machines.      │
│   Presents MS-DOS 6.22-compatible behavior.                      │
│                                                                  │
│   A Tombatossals Softworks product.                              │
│   Created by   Dave Abellan                                      │
│               Claudio di Castello                                │
│                                                                  │
│   Built on FreeDOS (GPLv2+) and other free/open software.        │
│   Castalia code & branding: MIT.  FreeDOS: GPLv2+ (third_party). │
│                                                                  │
│   (C) 2026 Tombatossals Softworks.                               │
│                                                                  │
│   Press any key to return to the keep.                           │
└──────────────────────────────────────────────────────────────────┘
```

The About screen names the studio (**Tombatossals Softworks**) and the creators
— Dave Abellan and Claudio di Castello — so the product
carries a clear, professional authorship line. The FreeDOS/GPL attribution
stays on this screen too (see §23.11 and `docs/LICENSE-STRATEGY.md`).

Pressing **C** on the About panel rolls the **credits** — a demoscene-style
scroll of the wordmark, the studio, and both creators (with their roles)
over a twinkling starfield, ending on a warm *GRACIAS*. It is skippable with any
key and is also reachable straight from the command line with `CASTALIA
/CREDITS`, which makes it easy to show at a demo or capture for the press.

The version line is the canonical form used everywhere a full version is shown:

```
CASTALIA DOS 386SX Edition — 1.0 "Tombatossals"
```

Short forms:

| Context                    | Form                                             |
|----------------------------|--------------------------------------------------|
| Full (About, docs)         | `CASTALIA DOS 386SX Edition — 1.0 "Tombatossals"`|
| Title bars                 | `Castalia DOS 1.0`                               |
| `/VER` one-liner           | `Castalia DOS 386SX Edition 1.0 (Tombatossals)`  |
| Status line                | `CASTALIA 1.0`                                    |

Honesty note baked into the copy: the About screen states plainly that the
FreeDOS kernel reports 7.x while Castalia *presents* MS-DOS 6.22-compatible
behavior. We do not claim a global 6.22 spoof; per-program version needs are
handled by SETVER (documented elsewhere in the bible).

---

## 23.6 Setup welcome message

The first screen of the Castalia installer. Gray panel, amber title, calm and
brief. It sets expectations and reassures the user that nothing is touched
without consent.

```
┌─ Castalia DOS Setup ─────────────────────────────────────────────┐
│                                                                  │
│   Welcome.                                                       │
│                                                                  │
│   This program installs CASTALIA DOS 386SX Edition on your       │
│   computer. Castalia is a DOS-compatible operating environment   │
│   built on FreeDOS and other free and open software, tuned for   │
│   real 386-class machines and the games they run best.           │
│                                                                  │
│   Setup will:                                                    │
│     •  Check your hardware and free disk space.                  │
│     •  Copy the system files and Castalia tools.                 │
│     •  Write a boot menu with memory profiles for gaming.        │
│     •  Keep a backup of any file it replaces.                    │
│                                                                  │
│   Nothing is changed until you confirm. You can stop at any      │
│   time by pressing Esc; your disk is left as it was.             │
│                                                                  │
│         [ Continue ]        [ Read the notes ]        [ Exit ]   │
│                                                                  │
│   Enter = Continue      Esc = Exit                               │
└──────────────────────────────────────────────────────────────────┘
```

Tone check: welcoming without gushing, explicit about safety ("Nothing is
changed until you confirm"), and it names the audience directly ("real 386-class
machines and the games they run best").

---

## 23.7 Help system tone

The Castalia help system reads like a good service manual: short paragraphs,
concrete steps, no jargon it hasn't defined, and a fixed header so the reader
always knows where they are. Help pages live in `C:\CASTALIA\HELP`.

**Tone.** Instructional and steady. Assume intelligence, not prior knowledge of
Castalia specifics. Prefer numbered steps for procedures. End tricky topics with
a one-line "If this doesn't work" pointer, never a shrug.

**Sample help header** (used at the top of every page; amber title, cyan status
line at the bottom of the screen):

```
┌─ Castalia Help ─── Memory Profiles ──────────────────── Page 1 of 3 ─┐
│                                                                     │
│   Memory profiles decide how much conventional memory a game gets   │
│   and which extras (XMS, EMS, CD-ROM) are available. You choose a    │
│   profile at the boot gate, or switch later with MEMPROF.           │
│                                                                     │
│   Quick guide                                                       │
│     •  Most DOS games ............ XMS Gaming                       │
│     •  Origin / Ultima, some Sierra  EMS Gaming                     │
│     •  CD-ROM games .............. CD-ROM Gaming                    │
│     •  A game misbehaves ......... Maximum Compatibility (CLEAN)    │
│                                                                     │
│   If a game still will not start, try Maximum Compatibility first;  │
│   it gives the most free conventional memory and loads no drivers.  │
│                                                                     │
├─────────────────────────────────────────────────────────────────────┤
│  F1 Help   PgUp/PgDn Pages   ↑↓ Scroll   Esc Back to the keep        │
└─────────────────────────────────────────────────────────────────────┘
```

A few reusable help phrasings:

| Situation             | Castalia help line                                       |
|-----------------------|----------------------------------------------------------|
| Intro to a tool       | "MEMPROF switches memory profiles without editing files." |
| A safe next step      | "If unsure, choose Maximum Compatibility and try again."  |
| A limit, stated kindly| "On a 386SX, expect this step to take a few seconds."     |
| A cross-reference     | "See Sound Setup for choosing a card and IRQ."            |

---

## 23.8 Error message tone

Error messages are where a brand is made or broken. Castalia's rule is absolute:
**describe the situation, name the next step, never blame the user, never shout.**
No stacked exclamation points, no all-caps panic, no cryptic codes without a
sentence to go with them.

Below are Castalia-voice messages paired with the terse, unhelpful DOS-style
message each one replaces. (The right column is the *generic old-DOS style*, not
copied product text.)

| Castalia message                                                        | Replaces (old DOS-ish style)          |
|-------------------------------------------------------------------------|---------------------------------------|
| "The drive is not ready. Insert a disk and press R to retry."           | "Not ready reading drive A / Abort, Retry, Fail?" |
| "That file could not be found. Check the name and path, then try again."| "File not found"                      |
| "There is not enough free memory to start this game. Try the XMS profile for more conventional memory." | "Insufficient memory" / "Out of memory" |
| "This command was not recognized. Type HELP to see what is available."  | "Bad command or file name"            |
| "The disk is full. Free some space or choose another drive, then retry."| "Insufficient disk space"             |
| "No sound card was detected. Sound is set to PC Speaker for now."       | "No sound device present"             |
| "That path does not exist. Create it first, or pick a folder that does."| "Path not found"                      |
| "The file is write-protected and was not changed. Remove protection to edit it." | "Access denied" / "Write protect error" |
| "The CD-ROM driver is not loaded. Boot the CD-ROM Gaming profile to read discs." | "CDR101: Not ready reading drive" |
| "Setup could not write CONFIG.SYS. Your original file is safe in C:\CASTALIA\BACKUP." | "Write fault error writing device" |
| "The chosen IRQ is already in use. Pick a different IRQ in Sound Setup." | "Device conflict"                     |
| "This game expects EMS memory, which the current profile does not provide. Switch to the EMS profile and try again." | "EMS not found / EMM386 not installed" |

Anatomy of a good Castalia error (use as a template):

```
[ what happened, plainly ] + [ the next concrete step ] ( + optional: where a
backup or safe copy is )
```

Formatting rules:

- One or two short sentences. It has to fit on a narrow screen and be read at a
  glance.
- Offer the keystroke or the tool that fixes it ("press R to retry", "Sound
  Setup", "the EMS profile").
- Reassure when the system protected the user's data ("your original file is
  safe").
- Never end an error with just a code. If a code is needed for support, append
  it after the sentence: `... (E12: write protect).`
- Errors are **Light Red 12 on the gray panel** (`0x7C`); truly critical,
  can't-continue banners use **White on Red** (`0x4F`) and are rare.

---

## 23.9 Logo direction (text mode)

The Castalia mark is a **castle keep** rendered entirely in CP437 box-drawing and
block characters, so it displays identically on 16-color VGA and on monochrome
(MDA/Hercules or amber/green CRT) text displays. No bitmap is required; the logo
*is* text. Every variant below stays within 80 columns and uses only CP437-safe
characters (single/double box lines, half/solid blocks, `■`).

**Color guidance.** On a 16-color display the keep body draws in **Light Gray 7 /
White 15** (steel walls), the merlons and the flagstaff accent in **Yellow 14**
(amber), all on the **Blue 1** desktop. On monochrome it renders in the single
available foreground color and still reads clearly because it relies on shape,
not color.

### Variant A — compact header mark (fits a title bar / menu header)

Roughly 22 columns wide, 5 rows tall. Use at the top-left of the Castalia main
menu and in tool headers.

```
   ▄   ▄   ▄
  ███ ███ ███      C A S T A L I A   D O S
 ┌───────────┐
 │ ▄ ▄ ▄ ▄ ▄ │     A fortress for your machine
 └───┴───┴───┘
```

Even more compact, single-line lockup for tight status bars:

```
 [█▀█] CASTALIA DOS
```

### Variant B — larger boot / splash keep (within 80 columns)

Used on the About panel and optional splash. The three-tower keep sits on a
stone base; merlons (the notched top) are the recognizable silhouette.

```
              ▄▄▄         ▄▄▄▄▄▄▄         ▄▄▄
             █████       █████████       █████
          ▄▄▄█████▄▄▄▄▄▄▄█████████▄▄▄▄▄▄▄█████▄▄▄
          ███████████████████████████████████████
          ██ ▄▄ ██████ ▄▄ ██ ▄▄ ██████ ▄▄ ███████
          ██ ██ ██████ ██ ██ ██ ██████ ██ ███████
          ██▄▄▄▄██████▄▄▄▄██▄▄▄▄██████▄▄▄▄████████
          ███████████████████████████████████████
          ██████ ▟█▙ █████████████ ▟█▙ ███████████
          ██████ ███ ████ ▄▄▄ ████ ███ ███████████
          ███████████████ ███ ███████████████████
        ▄▄███████████████████████████████████████▄▄
        ███████████████████████████████████████████
        ▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀

                 C A S T A L I A   D O S
                    386SX  Edition
```

Pure box-drawing variant (renders on the strictest monochrome text terminals;
no block glyphs at all):

```
      ___          _______          ___
     |###|        |#######|        |###|
   __|###|________|#######|________|###|__
  |#####################################|
  |# [] ###### [] ## [] ###### [] #######|
  |#####################################|
  |########  /\  #########  /\  #########|
  |########  ||  #### ## ##  ||  ########|
  |##############  ####  ################|
 _|#####################################|_
|#########################################|
'-----------------------------------------'
              CASTALIA  DOS
```

### CP437 character inventory (safe set)

Every logo above uses only these, all present in code page 437 and drawable in
text mode:

| Purpose        | Characters                                   |
|----------------|----------------------------------------------|
| Solid block    | `█`                                          |
| Half blocks    | `▄` `▀` `▌` `▐`                              |
| Small triangle | `▟` `▙` (corner blocks)                      |
| Square marker  | `■`                                          |
| Single lines   | `─ │ ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼`                      |
| Double lines   | `═ ║ ╔ ╗ ╚ ╝ ╠ ╣ ╦ ╩ ╬`                      |
| ASCII fallback | `# | _ / \ [ ] ( ) . ' - =`                  |

Logo usage rules:

- **Silhouette first.** The notched merlon top and three towers are the mark. If
  a rendering constraint forces a cut, keep the merlons.
- **Never distort to fit color.** If only one color is available, use it; the
  shape carries the identity.
- **No drop shadows or gradients.** Text mode has none; don't fake them.
- **Amber is an accent, not the fill.** The keep is steel-gray/white; amber
  touches the merlons and the name, so the eye lands on "CASTALIA".
- **Keep proportions stable.** Don't stretch the compact mark to boot size or
  vice versa; use the variant made for the space.

---

## 23.10 Naming conventions

Consistent names make the product feel like one thing. These are the rules.

### Product and edition names

- The product is always written **CASTALIA DOS** (small caps / all caps in
  banners; "Castalia DOS" in running prose and title bars).
- Editions are named for the **target hardware class**, matching the flagship
  form:

| Edition name                       | Target                              |
|------------------------------------|-------------------------------------|
| **CASTALIA DOS 386SX Edition**     | Flagship — 386SX and up, low RAM.   |
| CASTALIA DOS 486 Edition (future)  | 486-class, more RAM/CD headroom.    |
| CASTALIA DOS Pentium Edition (fut.)| Early Pentium; larger game sets.    |

Edition names describe the *floor*, not a lock — the 386SX Edition runs happily
on a 486 or Pentium; it is simply tuned to be correct on the humblest target.

### Version codenames — rationale

Codenames are **fortress towns of Valencia and Castellón**, the same coast the
brand's fortress identity comes from. They are dignified, real places, and give
each release a memorable, non-childish handle that ties back to the Mediterranean
stone-and-steel identity. There is no trademark risk in naming a software release
after a public town or a folk hero, and it keeps the whole roadmap thematically
coherent.

| Version | Codename         | Note                                             |
|---------|------------------|--------------------------------------------------|
| 0.1     | **Almenara**     | Watchtower town, Castellón. First light.         |
| 0.2     | **Peñíscola**    | The sea-rock fortress. Early walls up.           |
| 0.5     | **Morella**      | Walled hill city. The keep takes shape.          |
| 1.0     | **Tombatossals** | Castellón's legendary giant-hero. The 1.0 stands.|
| 1.1     | **Montornés**    | Hill castle above Benicàssim. Refinement.        |

Rules: codenames advance with the roadmap and are never reused. A codename is
paired with its version in the full form (`1.0 "Tombatossals"`) and may stand
alone only in informal release notes once the version is unambiguous.

### Tool naming style

Castalia's own tools follow an **8.3, uppercase, purpose-first** convention so
they are legible in a `DIR` listing and unmistakably Castalia. Names are English,
functional, and terse; where a warmer Castilian alias exists it is documented but
the primary name stays plain.

| Tool          | Primary name    | Alias(es)                       | Purpose                    |
|---------------|-----------------|---------------------------------|----------------------------|
| Main menu     | `CASTALIA.EXE`  | —                               | The keep: text-mode menu.  |
| Game launcher | `LAUNCH.EXE`    | `GAMEVAULT.EXE`                 | Browse and start games.    |
| Memory switch | `MEMPROF.EXE`   | —                               | Switch memory profiles.    |
| Sound config  | `SETSOUND.EXE`  | —                               | Choose card, IRQ, DMA.     |
| Diagnostics   | `HWINFO.EXE`    | —                               | Report hardware.           |
| Config editor | `CFGEDIT.EXE`   | —                               | Edit CONFIG/AUTOEXEC.      |
| Rescue        | `SAFEBOOT.EXE`  | —                               | Recover a broken boot.     |
| Per-game cfg  | `GAMECFG.EXE`   | —                               | Per-title settings.        |
| File manager  | `CASTFM.EXE`    | `CFM.EXE`, `ALCAZAR.EXE` (1.1)  | Manage files (planned).    |

Conventions for future tools:

- **8.3 filename, ≤8 letters, uppercase.** It must read cleanly in a bare `DIR`.
- **Purpose in the name.** `MEMPROF`, `SETSOUND`, `HWINFO` — you can guess what
  they do. Avoid cute names that hide function.
- **Castilian aliases are allowed but secondary.** `ALCAZAR` (the citadel) may
  alias the file manager, but `CASTFM` stays the primary, documented name so
  scripts and docs are unambiguous.
- **No version numbers in filenames.** The tool is `MEMPROF.EXE` in every
  release; the About screen carries the version.
- **Third-party names are never renamed** to look Castalian. `HIMEMX.EXE`,
  `JEMM386.EXE`, `CTMOUSE`, `SHSUCDX`, `UIDE.SYS` keep their upstream names and
  live in `C:\DOS` / `third_party\`, clearly separate from Castalia originals.

---

## 23.11 Explicit "do not use" list

These are hard rules. Violating them creates legal and brand risk and is not
permitted anywhere in the product, the installer, the docs, or the artwork.

**Never use:**

- **No Microsoft branding of any kind.** No "MS-DOS", "Microsoft", "Windows"
  wordmarks, logos, the four-pane Windows flag, or any Microsoft trademark used
  as *our* branding. (We may factually state compatibility — "MS-DOS
  6.22-compatible behavior" — as a plain description, never as a logo or claim of
  origin.)
- **No IBM logos or trademarks** used as Castalia branding. "IBM-compatible" may
  be used only as the industry-standard factual descriptor of the hardware
  class, never stylized as the IBM mark.
- **No Microsoft (or IBM) code, binaries, ROM images, or driver files.** The
  system is FreeDOS plus open/free components only. No `MSDOS.SYS`, `IO.SYS`,
  `COMMAND.COM` from Microsoft, no Microsoft `EMM386`, `HIMEM.SYS`, `SMARTDRV`,
  `MSCDEX`, etc. We ship the open equivalents named in the components list.
- **No copied Microsoft manual text, help text, error strings, or dialog copy.**
  Every string in Castalia is original (that is the entire point of §23.4–23.8).
  Do not paste MS-DOS documentation, prompts, or messages.
- **No third-party game logos, box art, screenshots, or trademarks** used as
  Castalia branding or in the installer. Games are the user's own; Castalia does
  not bundle or brand them.
- **No fonts, icons, or artwork under incompatible licenses.** Art must be
  original or under a permissive license compatible with the project.
- **No implying endorsement.** Do not suggest Microsoft, IBM, id Software,
  Origin, Sierra, or any rights-holder made, blessed, or partnered on Castalia.
- **No look-alike branding.** Do not imitate the MS-DOS setup blue screen,
  Windows Setup, or any product's distinctive trade dress closely enough to
  cause confusion. Castalia's blue desktop, gray panels, amber accent, and keep
  logo are its *own* look, described in this section.
- **No childish or off-brand marks.** No cartoon mascots, comic-style fonts,
  emoji in system UI, or joke error faces. It undercuts the serious identity.

**Always safe (for reference):**

- FreeDOS and its GPLv2+ components (kept in `third_party\`, shipped with source).
- The open/free tools named in the components list (HIMEMX, JEMM386, CTMOUSE,
  UIDE.SYS, SHSUCDX, FreeDOS KEYB, an LGPL/free SmartDrive-style cache).
- Original Castalia code, copy, menus, palettes, and the text-mode keep logo in
  this section.
- Factual, descriptive compatibility statements phrased as plain text, not logos.

---

*End of Section 23 — Product Personality and Branding.*
