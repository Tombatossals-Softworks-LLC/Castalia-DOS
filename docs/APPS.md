# The Castalia Application Suite

CASTALIA DOS is not just a boot layer: it ships its own suite of the tools
DOS users actually lived in. This document maps the most-used classic DOS
applications to their Castalia counterparts, defines the suite's shared
architecture, and records what is deliberately out of scope.

## Classic DOS staples → Castalia counterparts

| Classic staple (what people used) | Castalia counterpart | Status |
|---|---|---|
| Norton Commander / XTree Gold | `CASTFM.EXE` file manager | ✅ prototype |
| MSD (Microsoft Diagnostics) / CheckIt | `HWINFO.EXE` diagnostics | ✅ prototype |
| Norton SI / Landmark Speed Test / CheckIt bench | `CASTMARK.EXE` inspector + benchmark | ✅ prototype |
| Norton Disk Doctor / ScanDisk (surface side) | `CASTDOC.EXE` surface verify | ✅ prototype |
| CHKDSK / ScanDisk (FAT logical side) | FreeDOS `CHKDSK` (kept; not duplicated) | ✅ bundled |
| FastCopy / file-rescue copiers | `CASTCOPY.EXE` diskette rescue & transfer | ✅ prototype |
| MemMaker / QEMM Optimize | `MEMPROF.EXE` + boot profiles (a better model) | ✅ prototype |
| QEMM / 386MAX | `JEMM386` (open, bundled) | ✅ third-party |
| DOSSHELL / menu front-ends | `CASTALIA.EXE` main menu | ✅ prototype |
| EDIT / QEdit (config editing) | `CFGEDIT.EXE` (boot files, safe) | ✅ prototype |
| Norton Rescue / boot disks | `SAFEBOOT.EXE` + emergency floppy | ✅ prototype |
| Sound setup utilities (SBSET etc.) | `SETSOUND.EXE` | ✅ prototype |
| Game front-ends / menus | `LAUNCH.EXE` + `GAMECFG.EXE` | ✅ prototype |
| Text games & toys (snake, puzzles, falling blocks, minesweeper) | `SNAKE.EXE`, `PUZZLE.EXE`, `ALMENA.EXE`, `MINAS.EXE` | ✅ prototype |
| Artillery duels / board games / puzzles / solitaire | `SIEGE.EXE`, `REVERSI.EXE`, `BARRELS.EXE`, `SOLITARE.EXE` | ✅ prototype |
| MSD-style "what DOS is this?" | `CASTID.EXE` kernel signature card (uses the Castalia kernel API) | ✅ prototype |
| CD audio players (bundled with sound cards) | `CDPLAYER.EXE` over MSCDEX | ✅ prototype |
| After Dark / screen savers | `SAVER.EXE` castle-at-night | ✅ prototype |
| Product demos / attract loops | `CASTTOUR.EXE` interactive guided tour | ✅ prototype |
| LapLink (serial transfer) | `CASTLINK.EXE` | ✅ prototype (untested on metal) |
| Boot splash (OEM logos) | `BANNER.EXE` animated castle | ✅ prototype |
| PKZIP / ARJ / LHA | Info-ZIP `UNZIP`/`ZIP` bundled (`fetch-payload.sh --with-archiver`) | ✅ third-party |
| General text editor (QEdit-class) | `CASTEDIT.EXE` (line-oriented; `.BAK` on save; save-as; new files) | ✅ prototype |
| DOS HELP / readme viewers | `HELP.EXE` topic reader + the help pages | ✅ prototype |
| Undelete | `UNDEL.EXE` — recovers by copying out to another drive; never writes to the source | ✅ prototype (untested on media) |
| Unformat | **out of scope** — it can only work by writing to the damaged disk | ✗ never |
| Sidekick-style TSR popups | **rejected** — violates the no-TSR rule | ✗ never |
| SpinRite-class MFM refresh | **out of scope** — too risky to imitate | ✗ never |

## The showcase trio (added for 0.5 "Morella")

### CASTMARK — System Inspector & Benchmark (`src/castmark/`)

The Castalia answer to CheckIt/Norton SI/Landmark. One screen: hardware
inspector on the left (CPU class via shared probes, **real FNINIT/FNSTSW
coprocessor detection** — meaningful on a 386SX + 387SX pair — memory, XMS/
EMS, VGA, floppy count, COM/LPT, DOS, profile), benchmark bars on the right,
animated run box below.

Five fixed-duration (~1.5 s) benchmarks timed by the BIOS tick counter:
CPU integer, FPU (skipped honestly when no coprocessor), memory copy,
text-video fill, and disk read. Results show gradient bars, deltas versus
the previous saved run (`CASTMARK.SCR` — perfect for measuring CLEAN vs XMS
profiles), and the **Castalia Index** (386SX/16 = 100). Raw numbers are
always displayed.

**The anchors are measured, not estimated.** `bench_base[]` in `CASTMARK.C`
now holds a real run on an 86Box machine configured to match the reference
386SX — 4 MB, **387SX fitted**, CLEAN profile:

| Benchmark | Reference 386SX/16 | Previous guess | Guess was off by |
|---|---|---|---|
| CPU integer | 38 kOps/s | 80 | 2.1× too high |
| FPU (80x87) | 136 kFLOP/s | 8 | 17× too low — the guess assumed no 387 |
| Memory copy | 6656 KB/s | 2800 | 2.4× too low |
| Video (text) | 20.1 screens/s | 15.0 | 1.3× too low |
| Disk read | 530 KB/s | 400 | 1.3× too low |

That machine therefore scores exactly 100 by construction. The disk anchor
is the least transferable of the five — it is an emulated IDE image, and a
real drive or a CompactFlash card will differ — so it is the one to
re-measure on metal.

**The video benchmark owns its workload.** It used to call the shared
`ui_fill()`, which made the score a measurement of the machine *and* of
whatever the toolkit happened to look like that release: when `UI.C` moved to
16-bit cell writes, every machine's video score would have roughly doubled and
the 20.1 screens/s anchor would have quietly stopped meaning anything.
`bench_vid()` now carries its own frozen byte-at-a-time fill of `B800:0000`,
which is what the anchor was measured against, so scores stay comparable
across releases and the toolkit is free to get faster. Folding it back into a
`ui_fill()` call means re-measuring the anchor on the reference machine. A machine with no coprocessor **skips** the FPU
benchmark rather than scoring zero, so the index there is the geometric
mean of the other four and stays comparable.

**Scale.** Every benchmark prints its own multiplier against the 386SX
baseline (`x4.5`, `x162`) beside a bar drawn on a **log scale** running from
¼× to 32×, and the index is the **geometric mean** of those five
multipliers, so a 386SX/16 scores exactly 100 and a machine twice as fast
everywhere scores twice as much. The first version used a linear bar
capped at 4× and an arithmetic mean with each ratio clamped at 400%; on the
first machine faster than a 486 that gave four full bars out of five and an
index pinned at 362, where a 486 and a Pentium would have been
indistinguishable. `tests/unit/test_castmark_scale.c` holds the arithmetic
to those properties.

**Rates are computed divide-first.** `units * 182 / (ticks * 10 * 1000)`
overflows a 32-bit `long` at about 11.8 million units per run — reachable
by a machine roughly a hundred times a 386SX. That produced an obvious
failure in one benchmark (1 kOps/s next to 585 MB/s of memory bandwidth)
and a *plausible but wrong* one in another (an FPU score six times too low,
from a wrapped intermediate). `krate()` and `kbps()` divide first; the unit
tests pin both the reproduced good case and the overflow case.

The FPU anchor is the one that most needed measuring: the original guess
assumed a 386SX with **no** 80387 and was therefore 17× low, which made any
machine with a coprocessor score absurdly. The reference machine has a
387SX, so 136 kFLOP/s is "a 386SX doing hardware floating point", and other
machines are compared against that.

**Video adapter** is identified by the shared `VIDDET` module rather than a
single "is it VGA?" question. `INT 10h AX=1A00h` returns a *display
combination code* that names the adapter outright, so that byte does the
classifying; a BIOS too old to answer it falls through to the VGA-only
`AH=1Bh`, then `AH=12h BL=10h` for EGA, then the equipment word.

`scripts/test-video.sh` runs the probe inside DOSBox-X against **eleven
emulated adapters** — MDA, Hercules, CGA, Tandy, PCjr, Amstrad, EGA, MCGA,
VGA, S3 SVGA and VESA — and asserts the name for each. QEMU only ever
offers VGA, so before this the non-VGA branches were code-reviewed and
nothing more.

**MCGA is reported separately from VGA**, which the first version got
wrong: MCGA answers the VGA-era BIOS call, so "did it answer?" calls it
VGA, but it has no 16-colour EGA modes at all — on a games machine that is
the difference between a title running and not.

No probe writes to a video port. A DAC read-back would be more certain
still, but this is a diagnostic run on other people's vintage hardware: a
wrong adapter name is a cosmetic fault, and writing into the I/O space of
an unidentified card is not.

### CASTCOPY — Diskette Rescue & Transfer (`src/castcopy/`)

Purpose-built for pulling files off aging diskettes: browse A:/B:, tag files
(`Space`, `*` for all), choose the destination (default `C:\GAMES`), then
copy with per-file and total progress bars, live KB/s, and a **read-back
VERIFY pass on by default**. A verify mismatch or write error deletes the
partial destination file — the hard disk never keeps a silently-corrupt copy.

### CASTDOC — Disk Doctor, surface side (`src/castdoc/`)

Read-only, BIOS-level (INT 13h) inspection: reports the drive's geometry
and floppy type (360K/1.2M/720K/1.44M — it tells your 5.25″ from your
3.5″), then verifies sectors track by track (`AH=04h`, with reset+retry)
painting a live green/red surface map. Floppies get a full scan; hard disks
default to a clearly-labelled sampled scan. Afterwards it shows an
error-code breakdown (CRC, address mark, seek, timeout…) and plain-language
advice for the question that actually matters: **is it the diskette or the
drive?** (Scan a known-good disk in the same drive to split the two; then
rescue with CASTCOPY.) FAT repair intentionally stays with FreeDOS CHKDSK.

## Suite architecture (the impeccable-architecture rules)

Shared modules in `src/common/` — every tool builds from the same bricks:

| Module | Provides | Used by |
|---|---|---|
| `UI.C/H` | 80×25 text UI, palette, boxes, `ui_hbar` gradient bars, `ui_editline`, `ui_ticks` (18.2 Hz), `ui_keywaiting` | everything |
| `INI.C/H` | allocation-free INI reader | menu, launcher, memprof, gamecfg, castmark |
| `DIRW.C/H` | portable dir scan (Turbo C `findfirst` / Watcom `_dos_findfirst`) + `dirw_mkdir` | castfm, castcopy |
| `CPUDET.C/H` | CPU class (AC/ID bit, CPUID) + FPU probe (FNINIT/FNSTSW), byte-encoded in `#pragma aux` (disassembly-verified); builds at plain `-0` like everything else | hwinfo, castmark |
| `LOGO.C/H` | the fortress keep, the compact crown and the block wordmark, plus animation hooks (`logo_keep_row`, `logo_*_windows`, `logo_*_flags`) | banner, setup, castalia |
| `SPK.C/H` | the PC speaker: blocking `spk_tone()`, the non-blocking `spk_note()`/`spk_poll()` pair a game loop needs, the house voices, and the mute levers (`CASTSOUND`, `spk_mute`, `spk_off`) | setsound, every minigame |

Rules every suite tool obeys: C89; fixed buffers, no heap; text mode only;
keyboard first, mouse never required; no TSRs; BIOS ticks for timing; backup
before any write to user files; honest labels for anything sampled,
provisional, or undetectable.

**Technical-debt ledger:** `CFGEDIT`/`GAMECFG` private inline editors →
migrated to the shared `ui_editline` (**done**; note the shared editor
starts the cursor at the end of the line). `CASTALIA.EXE` menu v2 with
submenus: **done**. No known duplication remains across the suite.

## Validation notes for real hardware (386SX + 387, 4 MB, 5.25″ + 3.5″)

1. `CASTMARK`: coprocessor row must read **PRESENT (FNSTSW probe)**; CPU row
   reads "80386 (SX/DX look alike to software)" — that is honest, not a bug.
2. `CASTDOC`: both floppy types must be identified; scan a good and a known-
   bad diskette in each drive to exercise the media-vs-drive advice.
3. `CASTMARK` baselines: **done** from an 86Box machine matching the
   reference 386SX (table above); a 387 was fitted. What is left is to
   re-measure the **disk** anchor on the real machine, since that is the
   one an emulator cannot stand in for.
