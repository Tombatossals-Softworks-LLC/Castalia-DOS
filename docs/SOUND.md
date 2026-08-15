# 9. Sound Configuration — `SETSOUND.EXE`

> CASTALIA DOS 386SX Edition — Technical Bible, Section 9
> Applies to: 0.1 "Almenara" and later. Component: `C:\CASTALIA\BIN\SETSOUND.EXE`.
> Generated config: `C:\CASTALIA\CFG\SOUND.BAT`. Patched data: `C:\CASTALIA\CFG\GAMES.INI`.

This section defines the design of `SETSOUND.EXE`, the Castalia sound-configuration
tool. It covers the supported sound profiles, the full meaning of the `BLASTER`,
`SOUND`, and `MIDI` environment variables, how the generated `SOUND.BAT` include is
wired into `AUTOEXEC.BAT`, how per-game sound is stored in `GAMES.INI`, an honest
account of what can and cannot be autodetected on real ISA hardware, a practical
setup flow, the 80x25 text-mode UI, and a troubleshooting matrix.

The governing principle from the Castalia bible applies here without exception:
**compatibility beats elegance.** A sound tool that guesses wrong and hangs a 386SX
at game load is worse than useless; a tool that ships a known-good default and makes
manual override fast and obvious is correct. `SETSOUND.EXE` is built to the second
standard.

---

## 9.1 Purpose and design goals

`SETSOUND.EXE` is a small, text-mode, keyboard-first configurator whose single job
is to describe the machine's audio hardware to DOS and to every game Castalia
launches. It does this by writing three environment variables — `BLASTER`, `SOUND`,
and `MIDI` — into a generated batch include, and by recording a per-game sound
choice in `GAMES.INI`. It does **not** load a TSR, it does **not** stay resident,
and it does **not** poke the card during normal operation. It runs, writes text
files, and exits.

Design constraints, all non-negotiable on the 386SX target:

| Constraint            | Rule                                                             |
|-----------------------|-----------------------------------------------------------------|
| Memory                | Real-mode, `< 64 KB` resident image, fixed buffers, no `malloc`. |
| Video                 | VGA 80x25 text mode, 16 colors. Degrades to mono cleanly.       |
| Input                 | Keyboard-first. Mouse optional, never required.                 |
| Persistence           | Writes plain ASCII files only. No binary registry, no INI blobs.|
| Side effects at boot  | None. `SETSOUND` never runs from `AUTOEXEC`. Only the include runs.|
| Hardware probing      | Opt-in, read-mostly, always cancellable, always overridable.    |
| Toolchain             | Open Watcom C (`wcl -0 -bt=dos -ml -os`), Turbo C friendly, C89. |

**Why keyboard-first and text-mode.** The tool must be usable before a mouse driver
is guaranteed to be loaded (e.g. from the `PROMPT` profile or a rescue shell), on
machines whose only pointing device is a jumper-configured serial mouse, and on the
`386SX` where every KB of conventional RAM matters. A clean 80x25 UI with arrow-key
navigation and single-letter accelerators satisfies all of these.

**What SETSOUND is not.** It is not a mixer, not a driver, and not an autoconfig
daemon. Volume mixing (where the card supports software mixing, e.g. SB Pro / SB16)
is left to the card's own mixer utility, invoked optionally from `SOUND.BAT`. Music
playback tests are the only place `SETSOUND` produces sound, and only on request.

**PC-speaker test tone (shipping in 1.0).** Pressing **`T`** on the main screen
sounds a short rising chime through the motherboard beeper (8253 timer channel 2,
gated at port `61h`). It always uses the PC speaker — the one output DOS can drive
on any machine, jumperless and driverless — so it confirms the speaker and timer
are alive regardless of the selected profile; real Sound Blaster audio is proven by
a game. The card-driven IRQ/DMA "play test tone" described in the auto-detection
section below is a separate, planned probe.

### The shared beeper module (`src/common/SPK.{C,H}`)

**How the beeper is verified.** Two tiers, because they reach different
things. `tests/unit/test_spk.c` compiles the real module on the host and
drives its state machine with a fake clock — mute, note deadlines, the BIOS
tick's midnight wrap — but on the host the port writes compile to no-ops,
so nothing there can tell a correct 8253 divisor from a wrong one.
`scripts/test-speaker.sh` closes that: `SPKTEST.EXE` sounds 220, 440, 880
and 1760 Hz inside DOSBox-X, SDL's disk audio driver captures everything
the emulated speaker produces, and the host measures the tones back out of
the samples with a Goertzel filter. All four land within 0.11%, and the
tail of the capture is asserted silent because leaving the speaker off on
exit is a rule this module states.

The test was checked for teeth the only way that means anything: changing
`PIT_HZ` from 1193182 to 1000000 makes every tone read ~19% sharp and all
four assertions fail, which is exactly the bug a host-only test would have
waved through.


That test tone grew into the suite's shared voice. `SPK` owns the beeper for
every Castalia program, so the port sequence lives in exactly one place:

| Call | Behaviour | Use it for |
|---|---|---|
| `spk_tone(freq, ticks)` | **blocks** for the whole note | a menu chime, a win panel |
| `spk_note(freq, ticks)` + `spk_poll()` | starts the note and returns; `spk_poll()` (once per loop turn) silences it when due | **anything inside a game loop** |
| `spk_blip/ok/bad/boom/fanfare()` | the house voices, so the whole suite sounds like one product | ordinary events |

The non-blocking pair is the point: a 386SX cannot afford to stop drawing a
boulder's arc to hold a note, so nothing in a game ever waits on the speaker.

**Silence is a first-class state.** Three levers, in order of scope:

1. `CASTSOUND=OFF` (or `NO`/`0`) in the environment — read by `spk_init()`, so a
   system can be quiet before any program starts. `SOUND.BAT` is the natural
   place to set it.
2. The **`S` key** inside each minigame, toggling `spk_mute()` live.
3. `spk_off()`, which every program calls before it exits — a Castalia program
   must never leave the cone droning.

While muted every entry point is a no-op, so callers need no `if` of their own.
Port I/O is compiler-guarded exactly as `CPUDET.C` does it (Open Watcom
`inp`/`outp`, Turbo C `inportb`/`outportb`, no-ops for the host syntax gate).
No interrupt is hooked and no TSR is installed, per the project rule.

**Separation from FreeDOS / GPL code.** `SETSOUND.EXE` is original Castalia code
(MIT). It talks to hardware and to DOS through the environment and through plain
files; it does not static-link any GPL component. The FreeDOS kernel and shell read
the environment `SETSOUND` produces exactly as any DOS would.

---

## 9.2 What SETSOUND writes, and where

`SETSOUND.EXE` owns exactly two files:

1. **`C:\CASTALIA\CFG\SOUND.BAT`** — a generated batch fragment that `SET`s the
   three audio variables. `AUTOEXEC.BAT` `CALL`s it during every non-`SAFE` boot.
   This is "the Castalia sound-config include."

2. **The `Sound=` field of each game section in `C:\CASTALIA\CFG\GAMES.INI`** —
   the per-game override, patched in place when the user configures a single game's
   audio from `GAMECFG.EXE`/`LAUNCH.EXE` or from `SETSOUND`'s per-game screen.

Before overwriting `SOUND.BAT`, `SETSOUND` copies the previous version to
`C:\CASTALIA\BACKUP\SOUND.BAK`. Before patching `GAMES.INI`, it copies it to
`C:\CASTALIA\BACKUP\GAMES.BAK`. This mirrors how `CFGEDIT.EXE` backs up
`CONFIG.SYS`/`AUTOEXEC.BAT`, and it is what `SAFEBOOT.EXE` restores from if a bad
sound setting makes a machine unbootable into the menu.

A freshly generated `SOUND.BAT` looks like this (comments included for the human who
opens it in `CFGEDIT`):

```bat
@ECHO OFF
REM ------------------------------------------------------------------
REM  Castalia DOS - generated sound configuration
REM  File   : C:\CASTALIA\CFG\SOUND.BAT
REM  Tool   : SETSOUND.EXE 0.5 "Morella"
REM  Profile: Sound Blaster 16
REM  Edited : hand-edits are preserved only until SETSOUND runs again.
REM ------------------------------------------------------------------
SET BLASTER=A220 I5 D1 H5 T6 P330 E620
SET SOUND=C:\CASTALIA\DRV\SB
SET MIDI=SYNTH:1 MAP:E
REM  Optional: uncomment to restore the card mixer to sane defaults.
REM  IF EXIST %SOUND%\SBMIXER.EXE %SOUND%\SBMIXER.EXE /P
```

For a machine with no card, the file is still generated, still `CALL`ed, and simply
sets a `None`/PC-speaker profile so that Castalia's launcher and games see a
consistent, defined state instead of an empty environment:

```bat
@ECHO OFF
REM  Castalia DOS - generated sound configuration (profile: PC Speaker)
REM  No Sound Blaster present. BLASTER intentionally left unset.
SET SOUND=
SET MIDI=
```

---

## 9.3 Supported sound profiles

`SETSOUND` presents a fixed, ordered list of profiles. Each profile is a named
bundle of "what to write." The user picks one for the machine (the global default in
`SOUND.BAT`) and optionally a different one per game (`GAMES.INI`).

| # | Profile              | Digital (PCM)        | Music (synth)        | Bus / era               |
|---|----------------------|----------------------|----------------------|-------------------------|
| 0 | None                 | none                 | none                 | any (silent)            |
| 1 | PC Speaker           | 1-bit PWM (CPU)      | 1-voice square       | any PC, pre-1987        |
| 2 | AdLib                | none                 | OPL2 FM, 9 voices    | ISA, 1987+              |
| 3 | Sound Blaster 1.5    | 8-bit mono, 23 kHz   | OPL2 FM, 9 voices    | ISA 8-bit, 1990         |
| 4 | Sound Blaster 2.0    | 8-bit mono, 44 kHz   | OPL2 FM, 9 voices    | ISA 8-bit, 1991         |
| 5 | Sound Blaster Pro    | 8-bit stereo, 44 kHz | OPL3 FM, 18-20 voices| ISA 8-bit, 1991-92      |
| 6 | Sound Blaster 16     | 16-bit stereo,44 kHz | OPL3 FM + MPU-401    | ISA 16-bit, 1992+       |
| 7 | General MIDI (later) | (host card's PCM)    | GM via MPU-401       | ISA/serial, later phase |
| 8 | Roland MT-32 (later) | (host card's PCM)    | LA synth via MPU-401 | ISA/serial, later phase |

General MIDI and Roland MT-32 are listed for completeness and are **planned for a
later phase** (target 1.0 "Tombatossals" and beyond). Their config rows below show
the intended `MIDI`/`BLASTER P` wiring so that `GAMES.INI` can already carry the
profile id, but `SETSOUND` in 0.x marks them "planned" and does not offer them as a
selectable global default until the MPU-401 detection path is validated.

### 9.3.1 Per-profile detail

Each profile below lists typical capabilities, the typical port/IRQ/DMA, the exact
`SET BLASTER` line `SETSOUND` writes for it, and which games and eras it fits.

#### Profile 0 — None

- **Capabilities:** silence. Every game is told "no sound card."
- **Ports:** none.
- **`SET BLASTER`:** *(not written; the variable is left unset)*
- **Fits:** headless boxes, servers, machines where audio is broken and must be taken
  out of the equation while diagnosing. Also the safe fallback when detection is
  ambiguous and the user declines manual entry.

#### Profile 1 — PC Speaker

- **Capabilities:** the motherboard's 1-bit speaker driven by timer channel 2. Beeps,
  simple square-wave melodies, and (with CPU-hammering PWM tricks) crude sampled
  sound. No DMA, no IRQ, no card. CPU-bound — audible cost on a 386SX.
- **Ports:** none exposed to games via `BLASTER`; games have their own PC-speaker code.
- **`SET BLASTER`:** *(not written)*
- **Fits:** everything from the early 1980s onward as a lowest-common-denominator
  fallback: early Sierra AGI titles, *Prince of Persia*, *Commander Keen* (speaker
  mode), anything that predates or omits AdLib. Always available, never great.

#### Profile 2 — AdLib

- **Capabilities:** Yamaha OPL2 (YM3812) FM synthesis, 9 voices, music and simple
  sound effects. **No digital audio** — no sampled voices, no PCM. Mono.
- **Ports:** OPL2 at `388h`/`389h` (the AdLib standard, fixed). No IRQ, no DMA.
- **`SET BLASTER`:** *(not written; AdLib needs no BLASTER string.)* Games detect the
  OPL chip at `388h` directly. `SETSOUND` still writes a `SOUND.BAT` that records the
  profile in a `REM` line for the launcher's benefit.
- **Fits:** the golden age of FM music, 1988-1991: *Wing Commander*, *Ultima VI*,
  early *Sierra* (SCI0/SCI1 AdLib driver), *Dune*, *Monkey Island* FM soundtrack.
  Choose this when the machine has a genuine AdLib or a card in AdLib-only mode.

#### Profile 3 — Sound Blaster 1.5

- **Capabilities:** OPL2 FM (AdLib-compatible) **plus** 8-bit mono digital audio via
  single-cycle DMA, up to ~23 kHz output. DSP ~2.x. Adds sampled voices/SFX on top of
  AdLib music. No stereo, no auto-init DMA on the earliest revisions.
- **Typical port/IRQ/DMA:** `A220 I7 D1` (many 1.5 cards shipped IRQ 7). 8-bit DMA
  channel 1. No 16-bit DMA.
- **`SET BLASTER`:** `SET BLASTER=A220 I7 D1 T1`
- **Fits:** 1990-1992 titles that want digital speech over FM music: *Wing Commander*
  with speech, *Secret of Monkey Island* (SB SFX), early id/Apogee. The oldest "real"
  Sound Blaster profile Castalia offers.

#### Profile 4 — Sound Blaster 2.0

- **Capabilities:** OPL2 FM plus 8-bit mono digital, now up to ~44 kHz playback with
  auto-initialize DMA (smoother streaming than 1.5). Still mono, still 8-bit. DSP ~2.x.
- **Typical port/IRQ/DMA:** `A220 I5 D1` (IRQ 5 became the common default here).
- **`SET BLASTER`:** `SET BLASTER=A220 I5 D1 T3`
- **Fits:** 1991-1993 mono-digital titles: *Doom* (mono SB), *Wolfenstein 3-D*,
  *Duke Nukem II*, most Apogee/3D Realms shareware. A very safe, very compatible
  baseline for early-90s DOS gaming on a modest machine.

#### Profile 5 — Sound Blaster Pro

- **Capabilities:** **stereo** 8-bit digital (~22 kHz stereo / ~44 kHz mono) plus, on
  the Pro 2, the OPL3 (YMF262) FM chip (18-20 voices, richer than OPL2). Hardware
  software-controllable mixer. DSP ~3.x.
- **Typical port/IRQ/DMA:** `A220 I5 D1`. Some Pro cards also live at `A240`.
- **`SET BLASTER`:** `SET BLASTER=A220 I5 D1 T4`
- **Fits:** 1992-1994 titles wanting stereo digital and better FM: *Doom* (stereo),
  *Descent*, *Star Wars: X-Wing*, LucasArts iMUSE FM. Note that "SB Pro" covers two
  variants (Pro 1 with dual OPL2, Pro 2 with OPL3); Castalia's Pro profile targets the
  Pro 2 behavior (`T4`) since that is the common survivor and the better FM chip.

#### Profile 6 — Sound Blaster 16

- **Capabilities:** **16-bit stereo** digital up to 44.1 kHz (CD quality), OPL3 FM,
  an onboard MPU-401 UART MIDI port (`P330`) for external General MIDI / MT-32, and on
  AWE cards an EMU8000 wavetable (`E620`). Uses **both** an 8-bit and a 16-bit DMA
  channel. DSP ~4.x.
- **Typical port/IRQ/DMA:** `A220 I5 D1 H5`, MPU-401 at `P330`.
- **`SET BLASTER`:** `SET BLASTER=A220 I5 D1 H5 T6 P330` (add `E620` on AWE cards).
- **Fits:** 1993 onward, the peak of DOS gaming: *Doom II*, *Duke Nukem 3D*,
  *Warcraft II*, *Quake* (SB16 digital), and anything that offers General MIDI through
  the card's own MIDI port. The most capable single-card profile Castalia ships.

#### Profile 7 — General MIDI (planned, later phase)

- **Capabilities:** 16-channel General MIDI music through an MPU-401 UART interface —
  either the SB16's onboard MPU (`P330`), a dedicated MPU-401 card, or a Roland
  SC-55/SCC-1 daughterboard/module. Digital audio still comes from the host SB card.
- **Typical port/IRQ:** MPU-401 at `330h` (UART mode; polled, IRQ often unused).
- **`SET MIDI`:** `SET MIDI=SYNTH:1 MAP:E` and `BLASTER ... P330` so games find the port.
- **Fits:** mid-90s titles with lush GM scores: *Doom II*, *Warcraft II*, *Tie Fighter*
  (GM), *Monkey Island 2* on a GM device. Deferred until the MPU-401 probe is validated.

#### Profile 8 — Roland MT-32 (planned, later phase)

- **Capabilities:** Roland LA synthesis via an MT-32/CM-32L/LAPC-I through MPU-401.
  The gold standard for late-80s/early-90s Sierra and LucasArts music, with custom
  instrument banks and reverb that GM cannot reproduce faithfully.
- **Typical port/IRQ:** MPU-401 at `330h`; LAPC-I is an ISA MPU at `330h`.
- **`SET MIDI`:** `SET MIDI=SYNTH:1 MAP:B` (base map; MT-32 is not GM-mapped).
- **Fits:** *Monkey Island*, *Ultima VI/VII*, *Space Quest III*, *King's Quest IV/V/VI*
  — anything authored for MT-32 first. Deferred with General MIDI.

---

## 9.4 The `BLASTER` environment variable, in full

Every DOS Sound Blaster game reads a single environment variable to learn where the
card is. That variable is `BLASTER`. The canonical Castalia teaching example is:

```
SET BLASTER=A220 I5 D1 H5 T4
```

It is a space-separated list of single-letter tokens, each a letter followed by a
value. Order does not matter to games, but Castalia always writes them in the order
`A I D H T P E` for readability. Here is every token:

| Token | Name              | Meaning                                          | Radix   |
|-------|-------------------|--------------------------------------------------|---------|
| `A`   | I/O base port     | Base address of the card's register file         | hex     |
| `I`   | IRQ               | Hardware interrupt line the card raises           | decimal |
| `D`   | 8-bit DMA channel | DMA channel for 8-bit digital transfers           | decimal |
| `H`   | 16-bit DMA channel| DMA channel for 16-bit transfers (SB16-class)     | decimal |
| `T`   | Card type code    | Numeric model class (see table below)             | decimal |
| `P`   | MPU-401 MIDI port | Base address of the onboard UART MIDI port        | hex     |
| `E`   | EMU8000 base      | Wavetable base address (AWE32/AWE64 only)         | hex     |

In the teaching example, `A220` = ports at `220h`, `I5` = IRQ 5, `D1` = 8-bit DMA 1,
`H5` = 16-bit DMA 5, `T4` = the SB Pro type class. `H` and `E` are only meaningful on
16-bit (SB16/AWE) cards; on an 8-bit SB Pro or earlier, `SETSOUND` omits `H`, and the
example's `H5` is shown only to illustrate the field. `P` (MPU-401) is written only
when the profile uses the card's MIDI port.

### 9.4.1 Card type codes (`T`)

`T` tells the game which digital/mixer feature set to assume. The Creative type codes
Castalia writes:

| `T` | Card class assumed              | Digital        | FM    | Notes                        |
|-----|---------------------------------|----------------|-------|------------------------------|
| `1` | Sound Blaster 1.5               | 8-bit mono     | OPL2  | Oldest supported SB.         |
| `2` | Sound Blaster Pro (rev 1)       | 8-bit stereo   | 2xOPL2| Dual-OPL2 Pro 1.             |
| `3` | Sound Blaster 2.0               | 8-bit mono     | OPL2  | Faster DMA than 1.5.         |
| `4` | Sound Blaster Pro (rev 2)       | 8-bit stereo   | OPL3  | Castalia's SB Pro profile.   |
| `5` | Sound Blaster Pro (MCV / PS-2)  | 8-bit stereo   | OPL3  | Micro Channel; rare.         |
| `6` | Sound Blaster 16 / AWE          | 16-bit stereo  | OPL3  | Adds `H`, `P`, optional `E`. |

Castalia never writes type codes above `6`; later AWE and clone-specific codes are
out of scope for the 386SX-era target. When in doubt, a lower, older type code is the
safer, more widely accepted value — most games treat an unfamiliar high `T` as "no
card," but almost all accept `T3`/`T4`/`T6`.

### 9.4.2 Common port / IRQ / DMA combinations

Real ISA cards are jumpered or soft-set to one of a small set of resource
combinations. `SETSOUND` offers exactly these as menu choices, defaulting to the
first row (the near-universal factory default):

| Field | Common values          | Default | Notes                                        |
|-------|------------------------|---------|----------------------------------------------|
| `A`   | `220`, `240`           | `220`   | Also `210 230 250 260 280` on some clones.   |
| `I`   | `5`, `7`, `10`         | `5`     | Also `2`/`9` (IRQ2 cascades to 9).           |
| `D`   | `1`, `3`               | `1`     | Also `0`. 8-bit DMA.                          |
| `H`   | `5`, `6`, `7`          | `5`     | SB16-class only; must differ from `D`.       |

Rules `SETSOUND` enforces before writing:

- `A` must be one of the allowed bases; `220` and `240` are offered by default.
- `I` must not collide with a known reserved IRQ. Castalia warns if the chosen IRQ is
  `7` (LPT1) or `5` (LPT2 / some SCSI) but does not forbid it — collisions are
  machine-specific.
- `D` (8-bit) is `0`, `1`, or `3`. `1` is the safe default.
- `H` (16-bit) is `5`, `6`, or `7` and must not equal `D`. It is written only for the
  SB16 profile.
- `P` (MPU-401) defaults to `330` when the profile uses external MIDI.

A game that is told the wrong port simply hears silence; a game told the wrong **DMA
or IRQ** can **hang** at load, because it programs the DMA controller / hooks the
interrupt and then waits forever for a transfer or IRQ that never comes on the real
channel. This asymmetry is why DMA/IRQ correctness matters more than port
correctness, and why the troubleshooting matrix (Section 9.11) treats a load-time
hang as a DMA/IRQ problem first.

---

## 9.5 `SET SOUND` and `SET MIDI`

Two companion variables round out the environment.

### 9.5.1 `SET SOUND`

```
SET SOUND=C:\SB16
```

`SOUND` names the **directory that holds the card's driver and utility suite** — the
mixer, the diagnose/init tool, the FM instrument banks. Creative's own installer sets
`SOUND=C:\SB16` (or `C:\SBPRO`, `C:\SB`) and its utilities read it to find their
files. Castalia keeps its bundled Creative-compatible utilities under its own tree,
so `SETSOUND` normally writes:

```
SET SOUND=C:\CASTALIA\DRV\SB
```

Games rarely read `SOUND` directly; it exists for the card's mixer/init tools (for
example an optional `SBMIXER.EXE /P` line in `SOUND.BAT` that resets the hardware
mixer to sane levels at boot). For the "None"/"PC Speaker"/"AdLib" profiles, `SOUND`
is written empty, because there is no digital driver directory to point at.

### 9.5.2 `SET MIDI`

```
SET MIDI=SYNTH:1 MAP:E
```

`MIDI` tells MIDI-aware DOS applications which synthesizer to use and how to map the
16 MIDI channels onto it. Two tokens:

| Token     | Name              | Meaning                                               |
|-----------|-------------------|-------------------------------------------------------|
| `SYNTH:n` | Synthesizer index | Which synth device to route MIDI to. `1` = the base   |
|           |                   | synth (OPL FM on an SB). `2` = an extended/secondary   |
|           |                   | synth (e.g. a wavetable or external GM module).        |
| `MAP:x`   | Channel map mode  | `E` = **extended** map: full General MIDI, channels    |
|           |                   | 1-16, used by GM devices. `B` = **base** map: MPC      |
|           |                   | base-level, music on channels 13-16, used for FM and   |
|           |                   | for non-GM devices such as the MT-32.                  |

Practical settings by profile:

| Profile            | `SET MIDI`                | Why                                          |
|--------------------|---------------------------|----------------------------------------------|
| None / PC Speaker  | *(unset)*                 | No MIDI device.                              |
| AdLib / SB 1.5-Pro | `SYNTH:1 MAP:B`           | FM only; base-level map on channels 13-16.   |
| SB 16 (FM music)   | `SYNTH:1 MAP:E`           | OPL3 as synth 1; extended map available.     |
| General MIDI       | `SYNTH:1 MAP:E`           | External GM via MPU-401; full 16-channel GM. |
| Roland MT-32       | `SYNTH:1 MAP:B`           | LA synth is not GM-mapped; base map.         |

The distinction that trips people up: `MAP:E` (extended/GM) versus `MAP:B` (base/FM).
An MT-32 driven with `MAP:E` will play the wrong instruments on the wrong channels;
a GM module driven with `MAP:B` will only sound on four channels. `SETSOUND` chooses
the correct map per profile so the user never has to reason about it.

---

## 9.6 `AUTOEXEC.BAT` integration

`SETSOUND` never edits `AUTOEXEC.BAT`. Instead, `AUTOEXEC.BAT` contains a single,
stable `CALL` to the generated include. This keeps the boot file human-readable and
lets `SETSOUND` regenerate audio config freely without ever rewriting boot logic.

Ready-to-paste `AUTOEXEC.BAT` snippet (place after memory/mouse setup, before the
menu launches):

```bat
REM ==================================================================
REM  Castalia DOS - audio environment
REM  SOUND.BAT is generated by SETSOUND.EXE. Do not hand-edit unless
REM  you accept that the next SETSOUND run will overwrite it.
REM ==================================================================
IF "%CASTPROFILE%"=="SAFE" GOTO SKIPSOUND
IF EXIST C:\CASTALIA\CFG\SOUND.BAT CALL C:\CASTALIA\CFG\SOUND.BAT
:SKIPSOUND
```

Notes on the wiring:

- **`CALL`, not plain execution.** Because `SOUND.BAT` only issues `SET` commands,
  `CALL` runs it in the current environment so the variables persist into the rest of
  `AUTOEXEC` and into every game launched afterward. A bare `SOUND.BAT` invocation
  would also work for a pure batch file, but `CALL` is explicit and correct and
  returns control cleanly.
- **`SAFE` profile skips sound.** The `SAFE` boot profile (bare kernel + shell, rescue
  only) deliberately does not touch audio, matching its "no drivers" contract from the
  boot-menu design. `DIAG` and `PROMPT` still `CALL` it so diagnostics and the bare
  prompt see the same environment a game would.
- **`IF EXIST` guard.** On a first boot before `SETSOUND` has ever run, the include
  may not exist; the guard prevents a "Batch file missing" error.
- **Profiles reference the same include.** The memory profiles (`CLEAN`, `XMS`, `EMS`,
  `CDROM`, `WIN3X`) all reach this common tail of `AUTOEXEC.BAT`, so all of them load
  the identical audio environment. Sound configuration is orthogonal to the memory
  profile: a user picks a memory profile at the boot menu and an audio profile in
  `SETSOUND`, and the two compose without interaction. (The only cross-effect worth
  noting: EMS/UMB layout does not change audio, but a card's driver TSR — if any — is
  loaded high by the same `LH` mechanism the rest of Castalia uses.)

The relationship at a glance:

```
   boot menu (CONFIG.SYS %CONFIG%)      SETSOUND.EXE (run any time)
            |                                    |
            v                                    v
      AUTOEXEC.BAT  ---- CALL ---->  C:\CASTALIA\CFG\SOUND.BAT
            |                                    |
            |                          SET BLASTER / SOUND / MIDI
            v                                    |
      CASTALIA.EXE / LAUNCH.EXE  <---- environment inherited
            |
            +-- per-game override read from GAMES.INI (Sound= field)
```

---

## 9.7 Per-game sound override in `GAMES.INI`

The `SOUND.BAT` include sets the **machine default**. Individual games sometimes need
a different profile — an MT-32 title on a machine whose global default is SB16, or a
cranky early game that only behaves in mono SB 2.0 mode. `SETSOUND` (and
`GAMECFG.EXE`) records this in the game's section of `GAMES.INI` via a `Sound=` field.

```ini
[DOOM]
Title=DOOM
Path=C:\GAMES\DOOM
Exe=DOOM.EXE
Profile=XMS
Sound=SB16          ; use the machine default SB16 profile

[MONKEY2]
Title=Monkey Island 2 - LeChuck's Revenge
Path=C:\GAMES\MI2
Exe=MONKEY2.EXE
Profile=EMS
Sound=MT32          ; override: force Roland MT-32 for this title

[KEEN1]
Title=Commander Keen - Marooned on Mars
Path=C:\GAMES\KEEN1
Exe=KEEN1.EXE
Profile=CLEAN
Sound=SPKR          ; PC speaker only; ignore the machine's SB
```

`Sound=` accepts a profile id: `NONE`, `SPKR`, `ADLIB`, `SB15`, `SB20`, `SBPRO`,
`SB16`, `GM`, `MT32`, or the literal `DEFAULT` (meaning "use whatever `SOUND.BAT`
set — do not override"). When `LAUNCH.EXE` starts a game whose `Sound=` differs from
the machine default, it re-`SET`s the audio variables for that child process only,
then restores them on return, so the override never leaks into the next game.

`SETSOUND` can be entered directly on a single game (`SETSOUND /G DOOM`) to edit just
that field; from the UI, the per-game screen (Section 9.10) lists games from
`GAMES.INI` and lets the user set the field with the same profile picker used for the
machine default. Patching preserves every other key and comment in the file — the
tool rewrites only the `Sound=` line of the named section (or inserts one directly
under the section header if absent), after backing the file up to `GAMES.BAK`.

---

## 9.8 Detection limits on real DOS — an honest account

It is tempting to promise "automatic sound detection." On real ISA hardware, that
promise cannot be kept safely, and Castalia does not make it. This subsection states
plainly what is and is not possible, and what `SETSOUND` actually does.

### 9.8.1 Why ISA Sound Blaster detection is unreliable

- **No enumeration bus.** Classic ISA has no equivalent of PCI configuration space or
  USB descriptors. There is no register that says "a Sound Blaster lives here." You
  find a card only by poking addresses and interpreting what pokes back — a heuristic,
  not a lookup.
- **Jumpered resources.** Most pre-PnP SB and clone cards set their port, IRQ, and DMA
  with **physical jumpers**. Software cannot read a jumper. The card is at whatever the
  jumpers say, and nothing on the card reports that back in general.
- **Clones lie or differ.** ESS, Aztech, OPTi, Crystal, and dozens of "Sound
  Blaster-compatible" chips answer the DSP probe with varying fidelity; some report a
  DSP version that implies features they only partly implement.
- **Probing has side effects.** Reading and writing DSP/mixer/DMA registers of a card
  that is *not* actually there, or is a different device at that address (a network
  card, a scanner interface), can wedge the bus or trigger a spurious interrupt. A
  probe that scans every possible base is a probe that can hang the machine it is
  trying to help.
- **IRQ/DMA can only be confirmed by trying.** Even once a DSP answers at a port, the
  only reliable way to learn its IRQ and DMA is to arrange a tiny transfer and see
  which interrupt/channel actually fires — an intrusive test that itself can hang if
  the guessed channel is wrong.

The honest conclusion: **on real DOS, a known-good default plus fast manual override
beats clever autodetection.** This is Castalia's stated policy.

### 9.8.2 What `SETSOUND` actually does at detection time

`SETSOUND` offers a **Detect** action that is deliberately conservative, read-mostly,
always cancellable, and never destructive to a working config. In order:

1. **Read the existing `BLASTER` variable.** If the environment already contains a
   `BLASTER` string (set by a prior `SOUND.BAT`, by a vendor installer, or by hand),
   `SETSOUND` parses it and pre-fills every field from it. This is the single most
   reliable "detection" available: trust the value that is already working. The user
   confirms or edits it.
2. **Read jumper-less PnP data where present.** On later ISA-PnP cards (SB16 PnP,
   AWE, and PnP clones), `SETSOUND` reads the ISA Plug-and-Play resource data through
   the standard PnP read-data port to learn the assigned base, IRQ, and DMA. This
   works only on genuinely PnP cards whose resources a PnP BIOS or an ISA-PnP init has
   already assigned; it is skipped silently on pure-jumper cards, which report nothing.
3. **DSP reset probe at `220h`, then `240h` (only).** For a jumpered card, `SETSOUND`
   performs the standard non-destructive DSP reset handshake at just the two common
   bases: write `1` then `0` to the reset register, read the read-status/data port, and
   check for the `0AAh` "ready" byte the SB DSP returns after reset. If it appears, the
   card's DSP is present at that base and its version byte is read to suggest a type
   code (`T`). `SETSOUND` scans **only** `220` and `240`, never the full range, to
   avoid poking unrelated hardware.
4. **IRQ and DMA are *not* auto-probed.** Because confirming them requires an intrusive
   transfer that can hang, `SETSOUND` does **not** silently trigger one. It fills IRQ
   and DMA with the profile's typical defaults (`I5 D1`, plus `H5` for SB16) and asks
   the user to confirm against the card's jumpers or manual. An optional, clearly
   labelled **"Play test tone"** action performs a single short DMA transfer with the
   chosen settings so the user can *hear* whether IRQ/DMA are right — the safe,
   opt-in equivalent of an IRQ/DMA probe, run only when the user asks and easy to abort.

Where every automatic step comes up empty (a jumpered clone at a nonstandard base,
say), `SETSOUND` falls back to **manual entry**: the user picks the profile, then the
port/IRQ/DMA from the menus of common values in Section 9.4.2, guided by the card's
jumper legend or manual. Manual is always available and never worse than a guess.

Summary of the detection ladder:

| Step | Method                         | Reliability | Applies to                   |
|------|--------------------------------|-------------|------------------------------|
| 1    | Parse existing `BLASTER`       | High        | Any already-configured box   |
| 2    | Read ISA-PnP resource data     | Medium-high | PnP cards only               |
| 3    | DSP reset probe @ 220/240      | Medium      | Jumpered SB/clone at 220/240 |
| 4    | Test tone (opt-in) confirms IRQ/DMA | Medium | User-driven, hearing test    |
| —    | Manual entry                   | Definitive  | Everything else; always avail|

---

## 9.9 Practical setup flow

The intended path for a user configuring audio on a fresh Castalia install:

1. **Launch.** From `CASTALIA.EXE` main menu choose **Sound Setup**, or run
   `SETSOUND` from any prompt. (No mouse required.)
2. **Detect.** Press **F5 / Detect**. `SETSOUND` parses any existing `BLASTER`, reads
   PnP data if present, and does the DSP reset probe at `220`/`240`. It reports what it
   found and pre-selects the matching profile and fields. If it finds nothing, it
   leaves the safe default (SB 2.0, `A220 I5 D1`) selected and says so.
3. **Pick the profile.** Confirm the detected profile or arrow to the correct one in
   the profile list (None → PC Speaker → AdLib → SB 1.5 → SB 2.0 → SB Pro → SB 16).
   The capability summary on the right updates as you move.
4. **Confirm resources.** Check `A`/`I`/`D`(/`H`) against the card's jumpers or manual.
   Change any field with left/right arrows to cycle the common values. For SB16, set
   the MPU-401 port (`P330`) if you will use external MIDI.
5. **Set MIDI/music mapping.** Accept the profile's default `SET MIDI` (base map for
   FM/MT-32, extended map for GM/SB16) or change it if you know better.
6. **Test.** Press **F9 / Play test tone**. A short FM chord and, for SB profiles, a
   brief digital sample play. If you hear both, IRQ and DMA are right. If music plays
   but the sample does not (or the machine stalls), fix DMA/IRQ and test again.
7. **Save.** Press **F2 / Save**. `SETSOUND` backs up the old `SOUND.BAT` to
   `SOUND.BAK`, writes the new one, and confirms. The change takes effect at the next
   boot; `SETSOUND` offers to apply the variables to the *current* session immediately
   as well.
8. **(Optional) Per-game overrides.** Press **F6 / Games**, pick a title, and set its
   `Sound=` field (e.g. `MT32` for a Sierra classic) different from the machine
   default. Save writes only that field into `GAMES.INI`.
9. **Exit.** **Esc / Exit**. Boot a game and enjoy. If something is wrong, the
   troubleshooting matrix (Section 9.11) maps the symptom to a fix.

If a saved setting ever makes the machine misbehave at boot, `SAFEBOOT.EXE` restores
`SOUND.BAK`, and the `SAFE` profile skips sound entirely so the machine always comes
up to a prompt.

---

## 9.10 Text-mode UI (80x25)

`SETSOUND.EXE` uses the standard Castalia 80x25 text UI: blue background (VGA color
1), light-gray/white framed panels, amber/yellow (14) highlights and field labels,
and a selected bar rendered black-on-amber. The layout is keyboard-driven; a status
line of function-key actions runs along the bottom.

### 9.10.1 Main screen — machine sound profile

```
+==============================================================================+
|  CASTALIA DOS   ~   SOUND SETUP  (SETSOUND)             0.5 Morella          |
+==============================================================================+
|                                                                              |
|  Sound profile (machine default)      Selected profile                       |
|  +----------------------------------+   +-------------------------------+    |
|  |   None                           |   | Sound Blaster 16              |    |
|  |   PC Speaker                     |   |                               |    |
|  |   AdLib (OPL2 FM)                |   | Digital : 16-bit stereo 44kHz |    |
|  |   Sound Blaster 1.5              |   | Music   : OPL3 FM + MPU-401   |    |
|  |   Sound Blaster 2.0              |   | Bus/era : ISA 16-bit, 1992+   |    |
|  | > Sound Blaster 16            <  |   | Fits    : Doom II, Duke3D,    |    |
|  |   General MIDI       (planned)   |   |           Warcraft II, Quake  |    |
|  |   Roland MT-32       (planned)   |   +-------------------------------+    |
|  +----------------------------------+                                        |
|                                                                              |
|  Resources                            Environment preview                    |
|  +----------------------------------+  +-----------------------------------+ |
|  | Port  (A)  [ 220 ]  240          |  | SET BLASTER=A220 I5 D1 H5 T6 P330 | |
|  | IRQ   (I)  [  5  ]  7  10        |  | SET SOUND=C:\CASTALIA\DRV\SB      | |
|  | DMA8  (D)  [  1  ]  0  3         |  | SET MIDI=SYNTH:1 MAP:E            | |
|  | DMA16 (H)  [  5  ]  6  7         |  |                                   | |
|  | MIDI  (P)  [ 330 ]               |  | -> C:\CASTALIA\CFG\SOUND.BAT      | |
|  +----------------------------------+  +-----------------------------------+ |
|                                                                              |
|  Up/Dn Profile   Tab Field   Lt/Rt Value                                     |
+==============================================================================+
| F2 Save  F5 Detect  F6 Games  F9 Test  F1 Help            Esc Exit           |
+==============================================================================+
```

- The `>  <` markers show the selected profile; on VGA it is a black-on-amber bar.
- The **Environment preview** panel live-renders exactly what will be written to
  `SOUND.BAT`, so the user always sees the literal `SET` lines before saving.
- Fields shown in `[ ]` are the current value; the greyed values to the right are the
  other common choices left/right arrows cycle through.

### 9.10.2 Detect result dialog

```
        +--------------------------------------------------------------+
        |  DETECT SOUND HARDWARE                                        |
        +--------------------------------------------------------------+
        |                                                              |
        |  Existing BLASTER var .... A220 I5 D1 H5 T6  (parsed)        |
        |  ISA PnP resource data ... not present                       |
        |  DSP reset @ 220h ........ OK  (DSP v4.05 -> SB16, T6)       |
        |  DSP reset @ 240h ........ no response                       |
        |                                                              |
        |  IRQ / DMA are NOT auto-probed (unsafe on ISA).             |
        |  Defaults applied: I5 D1 H5. Confirm against jumpers,        |
        |  then use F9 Test to verify by ear.                          |
        |                                                              |
        |  Suggested profile:  Sound Blaster 16                        |
        |                                                              |
        |            [ Use these ]        [ Enter manually ]           |
        +--------------------------------------------------------------+
```

### 9.10.3 Per-game override screen (F6)

```
+==============================================================================+
|  SOUND SETUP  ~  PER-GAME OVERRIDES  (GAMES.INI)                             |
+==============================================================================+
|  Game                                 Sound override                         |
|  +---------------------------------+   +------------------------------------+|
|  | > DOOM                       <  |   | > SB16   (machine default)         ||
|  |   Doom II                       |   |   Profiles:                        ||
|  |   Monkey Island 2               |   |   NONE  SPKR  ADLIB  SB15  SB20    ||
|  |   Commander Keen 1              |   |   SBPRO  SB16  GM  MT32  DEFAULT   ||
|  |   Warcraft II                   |   |                                    ||
|  |   Wing Commander                |   | Machine default = SB16             ||
|  +---------------------------------+   +------------------------------------+|
|                                                                              |
|  Up/Dn Game   Lt/Rt Override   Enter Apply to this game                      |
+==============================================================================+
| F2 Save  F3 Back to machine setup                        Esc Exit            |
+==============================================================================+
```

All three screens obey the same palette and the same accelerator conventions as the
rest of Castalia's tools (`CASTALIA.EXE`, `MEMPROF.EXE`, `CFGEDIT.EXE`): amber labels,
black-on-amber selection, F-keys along the bottom, `Esc` always safe (prompts if
unsaved changes exist).

---

## 9.11 Troubleshooting matrix

Sound problems on DOS almost always reduce to one of six symptoms. Because DOS games
usually configure **digital** (PCM SFX/voice via the SB DSP + DMA + IRQ) and **music**
(FM via OPL, or MIDI via MPU-401) as **two separate devices**, the single most useful
diagnostic question is: *which of the two works?* The matrix below is ordered from
that split.

| Symptom                        | Likely cause                                   | Fix                                                                 |
|--------------------------------|------------------------------------------------|---------------------------------------------------------------------|
| **No sound at all**            | `BLASTER` unset / not `CALL`ed; wrong `A` port; game set to "None"; mixer muted | Confirm `SOUND.BAT` is `CALL`ed in `AUTOEXEC`; check `SET BLASTER`; verify port matches jumpers; run the card mixer; re-run game's own setup and pick the card. |
| **Hangs at game load**         | **Wrong IRQ or DMA** — game programs DMA/hooks IRQ then waits forever | Fix `I`/`D`(/`H`) to match the card's jumpers; test with **F9**; try `I5 D1` (or `I7`) as the safe baseline; drop from SB16 to SB Pro/SB2 profile to rule out `H`. |
| **Digital OK, but no music**   | FM/OPL not selected in game; game using MIDI/MPU that isn't present; `SET MIDI` wrong | In the game's setup pick "AdLib/Sound Blaster" for music, not "General MIDI"; if GM intended, verify `P330` and an actual MPU/GM device; set `MAP:B` for FM. |
| **Music OK, but no FX/voice**  | Digital half misconfigured: wrong `D` DMA or `I` IRQ; game's digital device set to "None" | Enable "Sound Blaster" for *digital*/SFX in the game; correct `D`/`I`; confirm with **F9** test sample; check mixer's voice/PCM level. |
| **Stutter / crackle / choppy** | DMA channel shared/contended; CPU too slow for the sample rate; missing auto-init DMA on SB 1.5 | Use a different `D` channel; lower the game's sample-rate/quality; prefer SB 2.0+ profile (auto-init DMA); avoid loading heavy TSRs that steal cycles on the 386SX. |
| **DMA hang / lockup mid-game** | 16-bit `H` channel wrong or equal to `D`; two cards on same DMA; buggy clone auto-init | Set `H` to `5`/`6`/`7` and ensure `H != D`; if unsure, use the SB Pro (8-bit, no `H`) profile; check for a second device on that DMA channel. |

Cross-cutting checks that resolve many of the above at once:

- **Is `SOUND.BAT` actually running?** At a prompt, type `SET` and confirm `BLASTER`,
  `SOUND`, and `MIDI` appear. If not, the `CALL` in `AUTOEXEC` is missing or you booted
  the `SAFE` profile (which skips sound by design).
- **Do the numbers match the card?** The environment can say anything; the jumpers are
  the truth. When they disagree, the jumpers win — change the environment to match.
- **Does the game have its *own* setup?** Almost every DOS game ships `SETUP`/`INSTALL`
  that must independently be told the device. Castalia's environment is necessary but
  not always sufficient; the game's own config must also select the card.
- **Lower and simpler beats higher and fancier.** When a machine is flaky, step the
  profile *down* (SB16 → SB Pro → SB 2.0 → AdLib). Fewer resources (no `H`, mono,
  OPL2) means fewer ways to be wrong. Once it works, step back up if desired.

---

## 9.12 386SX realities and design constraints

A closing, honest note consistent with the rest of the bible:

- **The PC speaker costs CPU.** On a 386SX, speaker PWM sampled sound steals real
  cycles from the game. It is the universal fallback, not a good default when any FM
  card is present.
- **FM is cheap, digital is not.** OPL FM music costs the game almost nothing; digital
  playback needs DMA bandwidth and IRQ servicing. On the slowest targets, choosing FM
  music (AdLib/OPL) plus modest 8-bit digital (SB 2.0) is often smoother than 16-bit
  44 kHz stereo, even on a card that supports it.
- **One correct config beats many clever ones.** `SETSOUND` ships the SB 2.0 default
  (`A220 I5 D1 T3`) because it is the single most widely accepted configuration across
  the 386SX-era catalogue. Everything else is an informed override from there.
- **Never let audio block the boot.** Audio config lives in an *included* batch file,
  is skipped in `SAFE`, is backed up before every write, and is restorable by
  `SAFEBOOT.EXE`. A wrong sound setting must never be able to keep a Castalia machine
  from reaching a prompt. That guarantee is the whole reason the design is shaped the
  way it is.

---

*End of Section 9 — Sound Configuration.*
*See also: Section on memory profiles (`CONFIG.SYS %CONFIG%`), `CFGEDIT.EXE`
(config editor), `SAFEBOOT.EXE` (rescue), and `GAMECFG.EXE` (per-game config).*
