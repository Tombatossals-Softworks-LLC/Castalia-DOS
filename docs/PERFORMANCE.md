# 19. 386SX Performance Budget

> **Section 19 of the Castalia DOS Technical Bible.**
> Flagship target: **CASTALIA DOS 386SX Edition**. Every design decision in
> this section is measured against a real Intel 386SX at 16 MHz with 1-2 MB
> of RAM, a slow IDE/CompactFlash disk, and no math coprocessor. If a feature
> is not pleasant on that machine, it is not shipped enabled by default.
>
> This budget floor is deliberately weaker than the machine `CASTMARK`
> calibrates against (a 386SX/16 with 4 MB and a 387; see
> `src/castmark/CASTMARK.C`): the benchmark needs a typical machine to
> score 100, the budget needs the worst one to stay pleasant.

The guiding rule of the whole project applies doubly here: **compatibility and
speed beat elegance.** A tool that starts instantly, uses almost no memory, and
never surprises the user is worth more than a beautiful tool that makes a 386SX
crawl. This section defines the hardware reality, the budget every Castalia tool
must live inside, and the rules that keep us honest.

---

## 19.1 The 386SX reality

The Intel 386SX is a 386DX core with its external interfaces cut down to save
cost. Software still sees a full 32-bit 386 CPU internally, but the chip talks
to the outside world through a narrower, slower path. That narrowing is the
single most important fact for performance work.

### 19.1.1 The 16-bit external data bus

The 386DX moves 32 bits per memory cycle. The 386SX moves **16 bits per memory
cycle** over a 16-bit external data bus, and addresses only 24 bits (16 MB
physical) instead of 32. Internally the SX registers are still 32-bit, so a
32-bit load or store that the DX finishes in one bus cycle costs the SX **two**
bus cycles.

| Trait                     | 386DX            | 386SX                       |
|---------------------------|------------------|-----------------------------|
| Internal registers        | 32-bit           | 32-bit (same core)          |
| External data bus         | 32-bit           | **16-bit**                  |
| Address bus               | 32-bit (4 GB)    | **24-bit (16 MB)**          |
| 32-bit memory access      | 1 bus cycle      | **2 bus cycles**            |
| Typical package           | 132-pin PGA      | 100-pin PQFP                |
| Practical RAM in the wild | 4-16 MB          | **1-4 MB**                  |

**What this means for us:** memory bandwidth, not raw clock, is usually the
bottleneck. Copying a buffer, scrolling a large region, or parsing a big file
all pay the 16-bit tax on every access. The fastest code on a 386SX is the code
that **touches the least memory**. We prefer small fixed buffers, byte- and
word-sized data, and algorithms that make one pass over as few bytes as
possible. We avoid gratuitous 32-bit data structures where a 16-bit one does the
job, because each wide access is a second bus cycle we did not have to spend.

### 19.1.2 Clock speed

386SX parts shipped at roughly **16, 20, and 25 MHz** (with 33 and 40 MHz
variants from other vendors later). Our reference machine is a **386SX/16** —
the slowest realistic target. If Castalia feels good on a 386SX/16, it feels
great on everything above it. We never tune to the fast end of the range and
hope the low end copes.

| Reference class     | Clock    | Rough character                          |
|---------------------|----------|------------------------------------------|
| Floor (must serve)  | 16 MHz   | Baseline for every budget number below   |
| Common              | 20 MHz   | Comfortable if the 16 MHz target is met  |
| Upper SX            | 25 MHz   | Headroom; never a design assumption      |

### 19.1.3 Usually no FPU

The 386SX almost never has a math coprocessor. The optional **387SX** was
expensive and rarely fitted, so we assume **no FPU is present.** Any floating
point in our code is emulated in software by the compiler's math library — tens
to hundreds of times slower than integer work, and it drags in kilobytes of
emulation routines we would rather not carry.

Therefore **Castalia tools do not use floating point.** Percentages, memory
sizes, timings, and layout math are all done in integers (fixed-point where a
fraction is truly needed). This keeps binaries small, keeps them fast, and keeps
behavior identical on a machine with a 387SX and one without.

### 19.1.4 Small RAM

1 MB is common; 2-4 MB is a lucky find; the SX tops out at 16 MB physical but
almost no period board was populated near that. Of that 1 MB, only the first
**640 KB** is conventional memory usable by real-mode DOS programs, and drivers,
the kernel, and TSRs all eat into it. Extended (XMS) and expanded (EMS) memory
exist behind the memory managers, but the thing a game actually needs — free
**conventional** memory below 640 KB — is scarce and contested. See §19.2.

### 19.1.5 Slow disk and floppy I/O

Period storage is slow and Castalia is often installed on a **CompactFlash card
through an IDE adapter** or on a small IDE hard disk. CF on a 386-era IDE bus is
convenient and silent but not fast, especially for many small reads (seek and
per-transaction overhead dominate). Floppies are slower still and mechanically
fragile.

| Medium                       | Realistic sustained read | Notes                          |
|------------------------------|--------------------------|--------------------------------|
| 1.44 MB floppy               | ~30-60 KB/s              | High latency; boot/rescue only |
| Period IDE HDD (386-era)     | ~0.5-2 MB/s              | Seeks are expensive            |
| CompactFlash via IDE adapter | ~0.5-3 MB/s              | Silent; small-read overhead    |

**What this means for us:** startup disk I/O is a first-class cost. A tool that
reads one small INI and its own code starts nearly instantly; a tool that scans
a directory tree, stats hundreds of files, or slurps a multi-hundred-KB database
at launch feels broken on CF. We minimize the number of reads at startup, read
sequentially, and never scan the disk unless the user asked for exactly that.

---

## 19.2 Conventional-memory pressure

DOS real-mode programs run in the low 640 KB. After the kernel, shell, and
whatever drivers a profile loads, what remains is the **free conventional
memory** number every DOS gamer watches. On Castalia the profiles are tuned to
protect it (see §Boot menu / memory profiles in the bible): **CLEAN** leaves
about **615 KB** free, and **XMS** — the default gaming profile — reaches
**~620-631 KB** by loading drivers into UMBs with no EMS page frame.

Those numbers are hard-won, and Castalia's own tools must not squander them.

### 19.2.1 Why every KB matters

Many period games have blunt, non-negotiable conventional-memory requirements.
A game that needs 600 KB free either runs or it does not; there is no graceful
degradation. If a Castalia TSR or an over-fed driver has quietly claimed 30 KB,
a game that should launch will refuse. The user does not see our elegant
architecture — they see "Not enough memory" and blame the whole product.

### 19.2.2 The cost of TSRs and drivers

Every resident program and driver is a permanent tax on conventional memory for
as long as it stays loaded. Rough period figures:

| Resident item              | Typical conventional cost | Verdict for Castalia            |
|----------------------------|---------------------------|---------------------------------|
| Mouse driver (CTMOUSE)     | ~3-5 KB (loaded high)     | Load high; optional, not forced |
| CD redirector (SHSUCDX)    | ~6-10 KB (loaded high)    | Only in CDROM profile           |
| Disk cache (SMARTDRV-style)| tens of KB                | Only where it earns its keep    |
| A careless resident helper | 10-40 KB, always          | **Forbidden by default**        |

The lesson: a resident helper that saves the user two keystrokes but costs 20 KB
forever is a bad trade on this hardware. Castalia's menu, launcher, and config
tools are **transient** — they run, do their job, and **exit completely**,
returning every byte before a game starts.

### 19.2.3 Why load-high and lean profiles are essential

Two techniques recover most of the pressure:

- **Load high** — with `DOS=HIGH,UMB` and a UMB provider (JEMM386), drivers and
  TSRs move into Upper Memory Blocks between 640 KB and 1 MB, freeing the low
  640 KB for the game. This is exactly why the XMS profile beats CLEAN on free
  conventional memory despite loading *more* drivers.
- **Lean profiles** — the profile ladder exists so the user loads only what a
  given game needs. No EMS page frame unless a game asks for EMS; no CD stack
  unless a CD game is being played; SAFE mode strips everything for rescue.
  Castalia's
  job is to make the leanest profile that still runs the target game the easy,
  default choice — and to never undermine it with resident bloat of our own.

---

## 19.3 Why VGA text mode is the right default UI

Castalia's UI is **VGA 80x25 text mode, 16 colors** — deliberately, not as a
placeholder for a "real" GUI later. On a 386SX this is the correct engineering
choice, and here is why.

| Property            | VGA text mode (80x25)            | 386SX graphical UI (e.g. 640x480x16) |
|---------------------|----------------------------------|--------------------------------------|
| Screen memory       | ~4 KB (2000 cells x 2 bytes)     | ~150 KB framebuffer, planar          |
| Redraw a full screen| Write 4 KB to B800:0000          | Blit/plane-shuffle ~150 KB           |
| Draw a character    | One word to video RAM            | Rasterize a glyph, plane writes      |
| Scroll a region     | BIOS/word moves, tiny            | Move tens of KB across a 16-bit bus  |
| Fonts/attributes    | Free in hardware ROM + attr byte | Software font rendering              |
| Redraw latency      | Effectively instant              | Visible on a 386SX                   |

**BIOS-fast and framebuffer-free.** In text mode the video hardware does the
compositing: each of the 2000 cells is a character byte plus an attribute byte,
and the VGA generates the pixels from its built-in font ROM. We write two bytes
and a glyph appears in the right color. There is no blitting, no plane
switching, no software font rasterizer, and no large framebuffer crossing the
16-bit bus on every update.

**One word per cell, and clip once.** A character byte followed by an
attribute byte *is* a little-endian 16-bit word, so `src/common/UI.C` stores
cells as words — 2000 stores for a full-screen repaint, not 4000 — and clips
each rectangle, run or string once on entry instead of bounds-checking every
cell on the way past. On a 16-bit ISA card that is the difference between one
bus cycle per cell and two, on the one code path every screen in the suite is
drawn through. `ui_putc()` keeps the per-cell test, because a single cell is
all it draws. `tests/unit/test_ui.c` holds the primitives to the output of the
per-cell version they replaced.

Note what this does *not* mean: `CASTMARK`'s video benchmark deliberately does
**not** call the toolkit. It carries its own frozen byte-at-a-time screen fill,
so that tuning `UI.C` moves the suite's drawing speed without silently moving
every machine's benchmark score away from the measured 386SX anchor.

**Tiny memory.** The whole screen is about 4 KB at segment `B800:0000`. A
graphical UI at 640x480 would need roughly 150 KB of planar video memory and a
matching amount of code to push pixels — memory and bandwidth the SX cannot
spare while also hosting a game.

**Instant redraw and snappy feel.** Because a full-screen repaint is a few
kilobytes, menus open, lists scroll, and panels refresh with no perceptible lag.
This is the "IBM-era seriousness" the visual identity calls for: calm, precise,
immediate. A graphical UI that stutters on a 386SX would feel *less* serious,
not more.

**A graphical UI should wait.** A GUI is not forbidden forever — it is a poor
*default* for the 386SX flagship. If a graphical shell ever ships, it belongs on
faster targets (486/Pentium) and as an option, never as the thing a 386SX user
is forced to run to reach their games. For 1.0 the text UI is the product.

---

## 19.4 Why protected-mode complexity should wait

It is tempting to build Castalia's tools as protected-mode (DPMI/DOS-extender)
programs to escape the 640 KB limit. For 1.0 "Tombatossals" we deliberately do
**not**, and stay in **16-bit real mode**. The reasons are compatibility, risk,
and cost.

| Concern                     | Real-mode tools (our choice)      | Protected-mode tools (deferred)      |
|-----------------------------|-----------------------------------|--------------------------------------|
| Memory-manager interaction  | Simple; coexists with any profile | Fights EMM386/JEMM for control        |
| Compatibility surface       | Small, well understood            | Extender/DPMI-host quirks per machine |
| 386SX suitability           | Native, light                     | Extender overhead, bigger footprint   |
| Dev + test cost             | Low                               | High (extender bugs are subtle)       |
| Failure mode                | Predictable                       | Hangs/conflicts hard to reproduce     |

**Compatibility risk.** Protected mode means a DOS extender and a DPMI host, and
both must cooperate with whatever memory manager the active profile installed.
Castalia's whole reason to exist is that games run; a tool that trips over a
user's EMM386 configuration and wedges the machine is a compatibility liability,
not an asset.

**Memory-manager conflicts.** JEMM386/EMM386, XMS, and a DOS extender all want
to arbitrate the same address space and CPU mode transitions. In real mode our
tools are guests that never contend for that control. The moment we go
protected, we own a class of "works here, hangs there" bugs across a huge range
of period hardware we cannot fully test.

**Development cost.** Real-mode C89 with Open Watcom, fixed buffers, and small
functions is buildable and testable today, and it is friendly to Turbo C too.
Protected-mode plumbing is a project of its own. For 1.0 that effort is better
spent on compatibility and polish. Protected mode may be revisited only if a
concrete tool genuinely cannot fit its data in real mode — and it must then earn
its complexity, not assume it.

---

## 19.5 Lightweight tools, and which features are dangerous

Castalia tools are small, single-purpose, transient real-mode programs. That is
not minimalism for its own sake; it is what the hardware rewards. The following
features are the classic ways a well-meaning tool ruins a 386SX, and each is
either banned or tightly constrained.

| Feature                    | Why it hurts a 386SX                                   | Castalia policy                          |
|----------------------------|-------------------------------------------------------|------------------------------------------|
| Background scanning        | Steals CPU + slow disk from foreground; CF thrash      | **Not allowed**; scan only on request    |
| Big in-memory caches       | Eats scarce conventional memory; 16-bit bus copies     | Small fixed buffers only                 |
| Animations / transitions   | Burns CPU and bandwidth for no function                | Off by default; instant redraw preferred |
| Resident helpers (TSRs)    | Permanent conventional-memory tax                      | Forbidden unless truly essential         |
| Polling loops              | 100% CPU spin; heats laptop CPUs, wastes cycles        | Block on BIOS/DOS input, never busy-wait |
| Floating point             | Software-emulated, slow, bloats the binary             | Integer/fixed-point only                 |
| Deep directory walks       | Hundreds of slow small reads at startup                | Read what you need, when you need it     |

**Background scanning** — an "index your games in the background" feature would
constantly wake the slow CF card and steal cycles from whatever the user is
doing. We scan only when the user explicitly asks (e.g. "rescan games"), show
progress, and stop.

**Big caches** — caching a whole games database in RAM to make the menu feel
snappy trades scarce conventional memory for a benefit text mode already
provides for free. We keep small fixed structures and re-read cheaply.

**Animations** — spinners, slides, and fades cost CPU and bus bandwidth to
accomplish nothing. A text screen that simply *appears* is faster and reads as
more serious.

**Resident helpers** — any always-loaded helper is a tax paid by every game for
as long as it lives. Our tools exit and return their memory instead.

**Polling loops** — a busy loop that spins the CPU checking for a keypress burns
100% of a precious 386SX and, on portables, cooks the battery. We block on BIOS
keyboard/DOS calls and yield.

---

## 19.6 The Castalia performance budget

Every Castalia tool is measured against the budget below on the **386SX/16
reference machine** (16 MHz, 2 MB RAM, CompactFlash-over-IDE, XMS profile).
These are targets a tool must meet to ship, not aspirations.

### 19.6.1 Per-tool budget (386SX/16 reference)

| Metric                              | Target on 386SX/16                         |
|-------------------------------------|--------------------------------------------|
| Cold start time (load → usable UI)  | **< 2 seconds**                            |
| Resident size when idle             | **0 KB** (tools are transient; no TSR)     |
| Conventional footprint while running| **< 96 KB** (code + data + stack)          |
| Disk reads at startup               | **≤ 3 reads** (own binary + 1 INI + 1 aux) |
| Full-screen redraw                  | **< 4 KB written**, effectively instant    |
| Floating-point operations           | **0**                                      |
| Heap allocations at runtime         | **0** (fixed buffers only)                 |

### 19.6.2 Named tools — indicative targets

Sizes are conservative real-mode expectations, not measured guarantees; the
build must verify each on hardware. All are transient (resident = none) unless a
row says otherwise.

| Tool           | Cold start | Running footprint | Startup reads | Resident?          |
|----------------|-----------:|------------------:|--------------:|--------------------|
| `CASTALIA.EXE` |    < 2.0 s |          < 80 KB  |    3 (INIs)   | No                 |
| `LAUNCH.EXE`   |    < 1.5 s |          < 64 KB  |    2          | No (exits to game) |
| `MEMPROF.EXE`  |    < 1.0 s |          < 48 KB  |    2          | No                 |
| `SETSOUND.EXE` |    < 1.0 s |          < 48 KB  |    1          | No                 |
| `HWINFO.EXE`   |    < 2.0 s |          < 64 KB  |    1          | No                 |
| `CFGEDIT.EXE`  |    < 1.5 s |          < 64 KB  |    2          | No                 |
| `GAMECFG.EXE`  |    < 1.5 s |          < 56 KB  |    2          | No                 |
| `SAFEBOOT.EXE` |    < 1.0 s |          < 40 KB  |    1          | No                 |

Notes:

- `LAUNCH.EXE` must free essentially all of itself before handing control to a
  game; the game gets the full conventional budget the profile provides.
- `HWINFO.EXE` may briefly touch more of the machine (probing hardware) but must
  still start within 2 s and never leave anything resident.
- If any future tool *must* stay resident, it requires an explicit exception
  (see Rule 6) and a documented, load-high, sub-6-KB footprint.

---

## 19.7 Performance rules — mandatory

Every Castalia tool **MUST** obey these rules. They are testable and not
optional. A change that breaks one of these does not ship.

1. **Start under 2 seconds** on a 386SX/16 whenever at all possible. Measure it;
   do not assume it. Startup work that cannot meet this must be deferred until
   after the UI is on screen or moved behind an explicit user action.
2. **Text mode by default.** VGA 80x25, 16 colors. No graphical mode is required
   to use any tool. A graphical mode, if it ever exists, is an opt-in extra on
   faster hardware.
3. **Avoid large buffers.** Use small, **fixed-size** buffers sized to the task.
   No unbounded reads, no "load the whole file to be safe." Respect the running
   footprint budget in §19.6.
4. **Avoid unnecessary disk scans.** Read only the files a tool actually needs,
   sequentially, and only when needed. Never walk directory trees or stat many
   files at startup. Scanning happens only on explicit user request, with
   visible progress.
5. **No background daemons.** No tool runs work in the background, polls on a
   timer, or wakes the disk on its own. Tools act only while the user is using
   them, then stop.
6. **No resident programs unless absolutely necessary.** Tools are transient:
   run, do the job, **exit fully**, return all memory. A TSR requires a written
   exception, must load high, and must fit a documented tiny footprint
   (target < 6 KB conventional).
7. **No fancy animations by default.** No spinners, fades, slides, or decorative
   motion in the default configuration. Screens appear instantly. Optional
   flourishes, if any, are off unless the user turns them on.
8. **Mouse optional, never required.** Every action is reachable from the
   keyboard. `CTMOUSE` support is a convenience; a machine with no mouse driver
   loses nothing but the pointer.
9. **Single-pass INI parse.** Read each configuration file **once**, top to
   bottom, into fixed structures. No re-reading, no multi-pass parsing, no
   reformat-and-write-back on load.
10. **Fixed-size arrays; no dynamic allocation** on the hot path. Prefer static
    and stack storage with defined maximums (e.g. a capped game count). If a
    limit is hit, fail cleanly with a clear message — never grow unbounded.
11. **No floating point.** All math is integer or fixed-point. No `float`,
    `double`, or FPU/emulator dependency anywhere in a Castalia tool.
12. **Block, don't spin.** Wait for input via BIOS/DOS calls; never busy-wait a
    polling loop that pins the CPU.

---

## 19.8 How we measure

Budgets are only real if they are checked. Castalia uses three complementary
methods, and a number is not "met" until it has been seen on real hardware.

| Method                     | Tool / setup                       | What it tells us                          |
|----------------------------|------------------------------------|-------------------------------------------|
| Cycle-accurate emulation   | **86Box** (386SX profile, PCem too)| Repeatable, tunable timing during dev     |
| Real-hardware stopwatch    | Physical 386SX/16 + stopwatch      | Ground truth for start time and "feel"    |
| Conventional footprint     | **`MEM /C`** (and `MEM /C /P`)     | Exact bytes each program/driver uses      |

**Cycle-accurate emulation (86Box / PCem).** Day-to-day timing work happens in
86Box configured as a 386SX/16 with period-appropriate RAM and a CF/IDE image.
Cycle-accurate emulation gives repeatable measurements and lets us model the
16-bit bus penalty and slow disk realistically without wearing out real
hardware. DOSBox-X remains the fast functional-testing harness, but timing
claims come from 86Box, not DOSBox-X.

**Real-hardware stopwatch.** The final gate is a physical 386SX. We time cold
start (`ENTER` to usable UI) with a stopwatch, watch redraw and scroll for any
perceptible lag, and confirm the tool *feels* instant. Emulator timing guides
us; hardware timing decides.

**Conventional footprint with `MEM /C`.** After a tool runs (or while a resident
exception is loaded), `MEM /C` shows the exact conventional and upper-memory
usage per program and driver. This is how we verify the §19.6 footprint targets,
confirm transient tools have truly released their memory, and prove that
load-high placed a driver in a UMB rather than in the low 640 KB.

A tool is signed off on performance only when: 86Box shows it within budget, a
real 386SX/16 confirms the start time and feel, and `MEM /C` confirms the
footprint and that nothing unexpected stayed resident.

---

*Section 19 — 386SX Performance Budget. Part of the Castalia DOS Technical
Bible. Documentation licensed CC BY 4.0. © 2026 The Castalia DOS Project.*
