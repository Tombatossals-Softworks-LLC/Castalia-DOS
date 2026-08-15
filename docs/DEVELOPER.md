# 14. Developer Toolchain

This section defines the official toolchain for building CASTALIA DOS original
tools (`CASTALIA.EXE`, `LAUNCH.EXE`, `MEMPROF.EXE`, `SETSOUND.EXE`,
`HWINFO.EXE`, `CFGEDIT.EXE`, `SAFEBOOT.EXE`, `GAMECFG.EXE`, and the planned
`CASTFM.EXE`). Everything here targets **16-bit real-mode DOS** on the
386SX-first hardware family and must remain legally clean: free/open tools only,
no Microsoft-owned compilers, assemblers, or headers.

The rules are practical, not aspirational. Every command line below is meant to
be typed and to work. Where the 386SX imposes a real limit, we say so.

---

## 14.1 Toolchain comparison

We evaluated six candidate toolchains against the constraints that actually
matter for CASTALIA DOS: does it emit 16-bit real-mode `MZ` executables, is it
free and legal to obtain in 2026, can it cross-compile from a modern Linux /
Windows / macOS host, and is its runtime small enough for conventional memory on
a 386SX.

| Toolchain | Real vs protected output | Availability / legality (2026) | Cross-compile from modern host? | 16-bit real-mode DOS suitability | License | Verdict for CASTALIA DOS |
|---|---|---|---|---|---|---|
| **Open Watcom C/C++ (V2 fork)** | Both: 16-bit real-mode `MZ` **and** 32-bit protected mode (DOS/4G-style extender) | Free, open, actively maintained on GitHub (`open-watcom/open-watcom-v2`) | **Yes** — native Linux, Windows, and macOS host binaries | **Excellent.** tiny/small/…/large models, 8086→Pentium codegen, tight runtime | Sybase Open Watcom Public License 1.0 (OSI-approved, MPL-1.1 derivative) | **PRIMARY** compiler |
| **Turbo C 2.01** | Real-mode only (8086/80286) | Free download historically (Borland/Embarcadero "Antique Software"), but proprietary; redistribution restricted; download page intermittent | **No** — DOS-hosted; must run under DOSBox/emulator | Good, classic K&R/C89, but old codegen, no 386 opcodes, no C99 | Proprietary "museum" grant (free-to-use, not FOSS) | **SECONDARY** compatibility target |
| **Borland C++ (3.1 / 4.x / 5.x)** | Real-mode (3.1) and protected mode (4.x+ via DPMI/extender) | Not legally obtainable today; effective abandonware, no free grant | **No** — DOS/Windows-hosted | Good real-mode codegen (3.1) | Proprietary, commercial | **NOT USED** — legal risk |
| **DJGPP (GCC for DOS)** | **32-bit protected mode only** (requires a DPMI host, e.g. CWSDPMI) | Free, open, maintained | **Yes** — cross toolchains build cleanly on Linux | **Unsuitable:** cannot emit 16-bit real-mode; heavy runtime; needs a DPMI server resident | GPL (tools); mixed runtime | **NOT USED** for our real-mode tools |
| **NASM** | Assembler — emits whatever you request (16 / 32 / 64-bit) | Free, open, actively maintained | **Yes** — native everywhere | **Excellent** for standalone real-mode asm and C-callable leaf routines | BSD 2-Clause | **ADOPTED** for all standalone assembly |
| **MASM / TASM-style assembly** | Assembler — both real and protected | MASM is Microsoft-owned (proprietary); TASM is Borland, not legally distributable today | MASM: Windows-hosted; TASM: DOS-hosted | Good, but wrong license story for a legally-clean project | Proprietary (both) | **AVOIDED** — use NASM (or a free MASM-syntax clone only if unavoidable) |

Notes on the assembler row: if a contributor genuinely needs MASM *syntax* (for
pasting a period-piece listing), use a free clone such as **JWasm** or **UASM**
rather than Microsoft's `ML.EXE` or Borland's `TASM.EXE`. Do not commit anything
that depends on a proprietary assembler being installed.

---

## 14.2 Recommendation

**Primary compiler: Open Watcom C/C++ V2.** For every 16-bit real-mode Castalia
tool, Open Watcom is the default and the tool that CI builds with. The reasons
are concrete:

- **Free and legally clean.** OSI-approved license, no Microsoft code, safe to
  ship the runtime linked into MIT-licensed Castalia binaries. Using Open Watcom
  to compile does **not** impose copyleft on our source — unlike static-linking
  GPL code, it keeps the MIT/GPL separation described in the licensing section.
- **Actively maintained.** The V2 fork receives regular commits and releases;
  it is not a frozen relic like the vendor tools.
- **Cross-hosts on Linux, Windows, and macOS.** Developers build DOS `.EXE`
  files from their normal desktop — no DOS VM required in the edit/compile loop.
- **Emits real 16-bit real-mode `MZ` executables** across the whole 8086→Pentium
  range, with tiny/small/compact/medium/large/huge memory models and a small,
  redistributable C runtime that fits conventional memory on a 386SX.
- **Complete, self-contained build system.** `wcc`/`wpp` (compilers), `wlink`
  (linker), `wlib` (librarian), `wmake` (make), `wdis` (disassembler), and `wd`
  (debugger) all ship together and behave identically across host OSes.

**Standalone assembly: NASM.** Where C cannot reach — CPU detection, tight video
inner loops, interrupt service routines — write NASM. It is free (BSD),
cross-hosts everywhere, and its OMF object output (`-f obj`) links directly with
`wlink`.

**Secondary compatibility target: Turbo C 2.01.** We keep the C code
Turbo-C-friendly (strict C89, no Watcom-only syntax in shared headers) and
periodically confirm the tools still build under Turbo C 2.01 in DOSBox. This
protects portability, documents that Castalia code is not locked to one vendor,
and gives us a fallback that runs *on the target itself* if we ever need to
compile on a real DOS box. Turbo C is a **check**, not the daily driver: it has
no 386 codegen and no C99, so it never becomes the primary.

Borland C++, DJGPP, MASM, and TASM are explicitly **not** part of the toolchain.

---

## 14.3 Build environment setup

### 14.3.1 Installing Open Watcom V2 on Linux

Download the current V2 snapshot installer (or the portable archive) from the
project. The portable route is the most reproducible for CI and for pinning a
known-good build:

```sh
# Choose an install root you control
export WATCOM=$HOME/watcom

# Portable archive layout after extraction:
#   $WATCOM/binl     32-bit Linux host binaries (wcc, wlink, wmake, ...)
#   $WATCOM/binl64   64-bit Linux host binaries
#   $WATCOM/h        C/C++ headers (DOS + generic)
#   $WATCOM/lib286   16-bit libraries (real-mode DOS)
#   $WATCOM/lib386   32-bit libraries (protected mode)
```

Put the environment setup in your shell profile (or a sourced `env.sh` in the
repo). Open Watcom is driven almost entirely by three variables:

```sh
# ~/.profile  (or repo-local scripts/owsetup.sh)
export WATCOM=$HOME/watcom
export PATH=$WATCOM/binl64:$WATCOM/binl:$PATH
export INCLUDE=$WATCOM/h            # DOS 16-bit headers live here
export EDPATH=$WATCOM/eddat         # editor/help data (optional)
export WIPFC=$WATCOM/wipfc          # help compiler data (optional)
```

Verify:

```sh
wcc  -v          # 16-bit C compiler version banner
wlink            # linker; prints usage
wmake -h         # make; prints help
```

If `wcc` reports "cannot open file" for headers, `INCLUDE` is wrong — it must
point at `$WATCOM/h`. On 64-bit-only hosts the tools you actually need
(`wcc`, `wpp`, `wlink`, `wlib`, `wmake`) are all present in `binl64`.

### 14.3.2 Installing Open Watcom V2 on Windows / macOS

- **Windows:** run the V2 installer; it sets `WATCOM`, prepends `%WATCOM%\binnt`
  (and `binnt64`) to `PATH`, and sets `INCLUDE` to `%WATCOM%\h`. Confirm from a
  fresh `cmd`:

  ```bat
  echo %WATCOM%
  wcc && wmake -h
  ```

- **macOS:** use the macOS host archive; the layout mirrors Linux but host
  binaries live in `$WATCOM/bino64` (and `bino` for 32-bit). Set:

  ```sh
  export WATCOM=$HOME/watcom
  export PATH=$WATCOM/bino64:$PATH
  export INCLUDE=$WATCOM/h
  ```

The **key point**: only `WATCOM`, `PATH`, and `INCLUDE` change between hosts.
The compiler flags, the makefile, and the resulting DOS binaries are identical.

### 14.3.3 Cross-building DOS EXEs from a modern host

There is no cross-compiler *prefix* to worry about — Open Watcom's compilers are
target-selectable at the flag level. You pick the target with `-bt=dos` and the
CPU with `-0`/`-3`, and `wlink` writes a DOS `MZ` executable regardless of which
host you launched it from:

```sh
# From Linux/macOS/Windows, produce a 16-bit real-mode DOS EXE:
wcl -0 -bt=dos -ms -os -wx -za hwinfo.c -fe=HWINFO.EXE

# Confirm it is a DOS MZ binary:
head -c2 HWINFO.EXE      # -> "MZ"
```

`wcl` is the one-shot "compile and link" driver (analogous to `cc`); it invokes
`wcc` then `wlink` for you. For anything larger than a single translation unit
we drive `wcc` and `wlink` from `wmake` (Section 14.6).

Because the toolchain is deterministic across hosts, CI runs the exact same
`wmake` on a Linux runner that a developer runs locally, and the byte output is
the DOS executable that ships.

---

## 14.4 Source folder structure

The build mirrors the repository's existing `src/` layout. Each Castalia tool
owns one directory; shared code lives in `src/common` and is compiled once into
a static library (`common.lib`) that every tool links against. All intermediate
`.obj`/`.lib` files and final `.exe` files land in `build/` at the repo root.

```
Castalia-DOS/
├── Makefile              # top-level Open Watcom wmake (Section 14.6)
├── src/
│   ├── common/           # shared library: video, keyboard, INI parser,
│   │                     #   CPU detect, timer, string helpers  -> common.lib
│   ├── castalia/         # CASTALIA.EXE  main text-mode menu     (LARGE model)
│   ├── launch/           # LAUNCH.EXE    game launcher           (LARGE model)
│   ├── memprof/          # MEMPROF.EXE   memory-profile switcher (small model)
│   ├── setsound/         # SETSOUND.EXE  sound configurator      (small model)
│   ├── hwinfo/           # HWINFO.EXE    diagnostics             (small model)
│   └── setup/            # installer / SETUP                     (LARGE model)
├── build/                # all .obj, common.lib, and final .exe outputs
├── config/  help/  dist/  floppy/  scripts/  tests/  third_party/
```

Future tools get their own sibling directory under `src/` when work starts:
`src/cfgedit` (`CFGEDIT.EXE`), `src/safeboot` (`SAFEBOOT.EXE`),
`src/gamecfg` (`GAMECFG.EXE`), and `src/castfm` (`CASTFM.EXE`, planned for 1.1).
No tool reaches "up" into another tool's directory; the only shared surface is
`src/common`.

### Memory-model guidance

DOS real-mode programs live inside 64 KB segments. The memory model decides
whether pointers are **near** (16-bit offset, one 64 KB segment) or **far**
(segment:offset, reachable across the 1 MB address space), and it applies
independently to code and to data.

| Model | Flag | Code | Data | Use it for |
|---|---|---|---|---|
| tiny | `-mt` | near | near | Single 64 KB `.COM` (rarely — we ship `.EXE`) |
| **small** | `-ms` | near (1 seg) | near (1 seg) | Tiny utilities: `MEMPROF`, `SETSOUND`, `HWINFO` |
| compact | `-mc` | near | far | Small code, large data tables |
| medium | `-mm` | far | near | Large code, small data |
| **large** | `-ml` | far | far | `CASTALIA` menu, `LAUNCH`, `SETUP` |
| huge | `-mh` | far | far + >64 KB objects | Only if a single array exceeds 64 KB (avoid) |

**Why the menu and launcher use large:** `CASTALIA.EXE` and `LAUNCH.EXE` carry
substantial code (screen drawing, box/menu widgets, help paging, INI parsing,
the game database, profile logic) plus data tables (game entries from
`GAMES.INI`, per-game configs, help text). That comfortably pushes past a single
64 KB code segment and often past 64 KB of static data, so far code and far data
(large model) are required. The cost — far calls and far pointers are a few
bytes and cycles heavier — is irrelevant for a menu that spends its life waiting
on the keyboard.

**Why tiny utilities use small:** `MEMPROF`, `SETSOUND`, and `HWINFO` are single
-purpose. Their code and data each fit inside one 64 KB segment, so small model
gives them near pointers: smaller binaries, faster calls, less memory footprint.
On a RAM-starved 386SX, keeping a resident-ish helper lean matters.

**The 64 KB rule you must never forget:** even in large model, a *single* object
(one array, one struct, one `malloc`) cannot exceed 64 KB without huge model or
`halloc`. Keep every buffer, every table, and every static array under 64 KB.
If a data set is genuinely larger, stream it or page it — do not reach for huge
model, which slows every pointer operation program-wide.

---

## 14.5 Compiler flags

Open Watcom flags are terse; these are the ones that define a Castalia build.

| Flag | Meaning | Castalia usage |
|---|---|---|
| `-0` | Generate 8086/8088 instructions | **Default** for anything that must run on any DOS PC and for maximum portability |
| `-3` | Generate 80386 instructions (still real-mode) | Only for code we *know* runs 386-and-up and benefits from 32-bit ops in a tight loop |
| `-bt=dos` | Build target = DOS | Always, for our tools |
| `-ms` | Small memory model | Tiny utilities (`MEMPROF`, `SETSOUND`, `HWINFO`) |
| `-ml` | Large memory model | Menu, launcher, setup |
| `-os` | Optimize for size | **Default** — smaller binaries, less conventional RAM |
| `-ot` | Optimize for time (speed) | Only hot paths (e.g. video blit modules) where size is affordable |
| `-wx` | Maximum warning level | Always — catch every diagnostic |
| `-we` | Treat warnings as errors | Recommended in CI so nothing rots |
| `-za` | Strict ANSI C89, no Watcom extensions | **Default** — keeps code Turbo-C-compatible |
| `-za99` | Compile as C99 | Only where a module is Watcom-only *and* C99 pays off; breaks Turbo C compat |
| `-zq` | Quiet operation (no banner) | Convenience in scripted builds |
| `-d2` | Full symbolic debug info | Debug builds (see Section 14.8) |
| `-fo=` | Set output object name/path | Route `.obj` into `build/` |
| `-fe=` | Set output executable name (wcl) | Name the final `.EXE` |

Guidance on the two decisions people get wrong:

- **`-0` vs `-3`.** Default to `-0`. The 386SX runs 8086 code perfectly, and
  `-0` code also runs on every older machine and in every emulator. Reach for
  `-3` only in an inner loop that provably benefits from 32-bit registers, and
  only in a module you are certain never executes on a pre-386 CPU. Mixing is
  fine at the file level — build one hot module `-3` and the rest `-0`.
- **`-za` vs `-za99`.** Default to `-za` (strict C89). It is what keeps the
  code buildable under Turbo C 2.01 and honest about the DOS-era dialect.
  `-za99` is a deliberate, per-module exception, never a project default.

### Example command lines

Compile-and-link a small single-file tool with `wcl`:

```sh
# HWINFO.EXE — small model, size-optimized, 8086, strict C89, max warnings
wcl -0 -bt=dos -ms -os -wx -za src/hwinfo/hwinfo.c -fe=build/HWINFO.EXE
```

Compile one translation unit to an object with `wcc` (large model):

```sh
wcc -0 -bt=dos -ml -os -wx -za -fo=build/menu.obj src/castalia/menu.c
```

Link several objects plus the shared library into a DOS `MZ` executable with
`wlink`:

```sh
wlink system dos \
      name build/CASTALIA.EXE \
      file  {build/main.obj build/menu.obj build/ui.obj} \
      library build/common.lib \
      option quiet, map=build/CASTALIA.map
```

`system dos` selects the real-mode DOS `MZ` output format; `option map` emits a
link map that is invaluable when a symbol is missing or a segment overflows.

---

## 14.6 Worked wmake / Makefile fragment

The top-level `Makefile` at the repo root is an Open Watcom `wmake` file. It
builds `src/common` into `build/common.lib` first, then each tool into
`build/`. This fragment is complete and consistent — it is the shape the real
top-level makefile takes.

```make
# ===========================================================================
# CASTALIA DOS - top-level Open Watcom wmake makefile
# Build:  wmake            (all tools)
#         wmake hwinfo     (one tool)
#         wmake clean
# ===========================================================================

# --- Tools -----------------------------------------------------------------
CC   = wcc
AS   = nasm
LINK = wlink
LIB  = wlib

# --- Common flags ----------------------------------------------------------
# -0     8086 code (portable to any DOS PC / 386SX)
# -bt=dos build target DOS      -os optimize for size
# -wx    max warnings           -za strict ANSI C89 (Turbo C friendly)
CFLAGS_COMMON = -0 -bt=dos -os -wx -za -zq

# Per-model flags
CFLAGS_SMALL = $(CFLAGS_COMMON) -ms
CFLAGS_LARGE = $(CFLAGS_COMMON) -ml

# NASM: OMF object output for wlink, 16-bit real mode
ASFLAGS = -f obj

BUILD = build

# --- Default target --------------------------------------------------------
all : $(BUILD)/common.lib &
      $(BUILD)/HWINFO.EXE &
      $(BUILD)/MEMPROF.EXE &
      $(BUILD)/SETSOUND.EXE &
      $(BUILD)/CASTALIA.EXE &
      $(BUILD)/LAUNCH.EXE
    @echo All Castalia tools built into $(BUILD)/.

# --- Shared library (src/common -> common.lib) -----------------------------
COMMON_OBJS = $(BUILD)/video.obj  $(BUILD)/keyboard.obj &
              $(BUILD)/ini.obj    $(BUILD)/strutil.obj  &
              $(BUILD)/cpudet.obj

$(BUILD)/common.lib : $(COMMON_OBJS)
    @if exist $@ del $@
    for %i in ($(COMMON_OBJS)) do $(LIB) -q -b $@ +%i

# common C sources are small-model-safe (near) helpers
$(BUILD)/video.obj    : src/common/video.c
    $(CC) $(CFLAGS_SMALL) -fo=$@ $[@
$(BUILD)/keyboard.obj : src/common/keyboard.c
    $(CC) $(CFLAGS_SMALL) -fo=$@ $[@
$(BUILD)/ini.obj      : src/common/ini.c
    $(CC) $(CFLAGS_SMALL) -fo=$@ $[@
$(BUILD)/strutil.obj  : src/common/strutil.c
    $(CC) $(CFLAGS_SMALL) -fo=$@ $[@

# CPU detection is assembly (NASM), C-callable leaf routine
$(BUILD)/cpudet.obj   : src/common/cpudet.asm
    $(AS) $(ASFLAGS) -o $@ $[@

# --- HWINFO.EXE (small model) ----------------------------------------------
$(BUILD)/hwinfo.obj : src/hwinfo/hwinfo.c
    $(CC) $(CFLAGS_SMALL) -fo=$@ $[@

$(BUILD)/HWINFO.EXE : $(BUILD)/hwinfo.obj $(BUILD)/common.lib
    $(LINK) system dos name $^@ &
            file $(BUILD)/hwinfo.obj &
            library $(BUILD)/common.lib &
            option quiet, map=$(BUILD)/hwinfo.map

# --- MEMPROF.EXE (small model) ---------------------------------------------
$(BUILD)/memprof.obj : src/memprof/memprof.c
    $(CC) $(CFLAGS_SMALL) -fo=$@ $[@

$(BUILD)/MEMPROF.EXE : $(BUILD)/memprof.obj $(BUILD)/common.lib
    $(LINK) system dos name $^@ file $(BUILD)/memprof.obj &
            library $(BUILD)/common.lib option quiet

# --- SETSOUND.EXE (small model) --------------------------------------------
$(BUILD)/setsound.obj : src/setsound/setsound.c
    $(CC) $(CFLAGS_SMALL) -fo=$@ $[@

$(BUILD)/SETSOUND.EXE : $(BUILD)/setsound.obj $(BUILD)/common.lib
    $(LINK) system dos name $^@ file $(BUILD)/setsound.obj &
            library $(BUILD)/common.lib option quiet

# --- CASTALIA.EXE (LARGE model: menu is big) -------------------------------
$(BUILD)/main.obj : src/castalia/main.c
    $(CC) $(CFLAGS_LARGE) -fo=$@ $[@
$(BUILD)/menu.obj : src/castalia/menu.c
    $(CC) $(CFLAGS_LARGE) -fo=$@ $[@

$(BUILD)/CASTALIA.EXE : $(BUILD)/main.obj $(BUILD)/menu.obj $(BUILD)/common.lib
    $(LINK) system dos name $^@ &
            file {$(BUILD)/main.obj $(BUILD)/menu.obj} &
            library $(BUILD)/common.lib &
            option quiet, map=$(BUILD)/castalia.map

# --- LAUNCH.EXE (LARGE model) ----------------------------------------------
$(BUILD)/launch.obj : src/launch/launch.c
    $(CC) $(CFLAGS_LARGE) -fo=$@ $[@

$(BUILD)/LAUNCH.EXE : $(BUILD)/launch.obj $(BUILD)/common.lib
    $(LINK) system dos name $^@ file $(BUILD)/launch.obj &
            library $(BUILD)/common.lib option quiet

# --- Housekeeping ----------------------------------------------------------
clean : .SYMBOLIC
    @if exist $(BUILD)/*.obj del $(BUILD)/*.obj
    @if exist $(BUILD)/*.lib del $(BUILD)/*.lib
    @if exist $(BUILD)/*.map del $(BUILD)/*.map
    @if exist $(BUILD)/*.EXE del $(BUILD)/*.EXE
```

Notes on the `wmake` idioms used above:

- `$^@` is the **full path of the target** being built; `$[@` is the **full path
  of the first dependent** (the source). These are Watcom's forms — do not use
  GNU-make's `$<`/`$@` semantics here.
- `&` at end of line is `wmake`'s line-continuation character.
- `.SYMBOLIC` marks `clean` as a target that is not a file.
- `common.lib` is assembled from objects with `wlib -b` (batch/quiet); linking
  `library common.lib` pulls in only the members each tool references.

---

## 14.7 C coding style for DOS

The house style keeps code building on both Open Watcom and Turbo C 2.01, and
keeps it honest about real-mode constraints.

- **C89 only.** Declarations at the top of each block, `/* ... */` comments,
  no `//` line comments, no mixed declarations-and-code, no VLAs. This is what
  `-za` enforces and what Turbo C accepts.
- **No `long long`, no 64-bit types.** They do not exist in the DOS-era dialect
  and blow up code size in real mode. `int` is 16-bit; use `long` (32-bit) only
  when you genuinely need it.
- **Fixed buffers, avoid `malloc`.** Prefer static and stack buffers sized at
  compile time. Dynamic allocation in real mode fragments the tiny DOS heap and
  invites far-heap complexity. If you must allocate, allocate once at startup
  and never free in a loop. Every buffer stays **under 64 KB** — no single
  object may cross a segment.
- **Far pointers for hardware, only where needed.** Video memory is
  `0xB800:0x0000` (color text). Access it through a typed far pointer:

  ```c
  /* One 80x25 text cell = char + attribute byte */
  unsigned char far *vram = (unsigned char far *)0xB8000000UL;
  vram[(row * 80 + col) * 2 + 0] = ch;    /* character   */
  vram[(row * 80 + col) * 2 + 1] = attr;  /* attribute   */
  ```

  In large model most pointers are already far; in small model use the explicit
  `far` keyword for the video/BIOS pointers only, leaving everything else near.
- **BIOS/DOS via the interrupt intrinsics.** Use `int86`/`int86x` (and
  `intdos`/`intdosx`) with `union REGS` / `struct SREGS` from `<i86.h>` (Open
  Watcom) — the same shapes Turbo C exposes via `<dos.h>`. For 32-bit register
  access on a 386 there is `int386`/`int386x`, but keep those out of shared
  code that must also build under Turbo C.

  ```c
  #include <i86.h>
  union REGS r;
  r.h.ah = 0x0F;          /* BIOS: get current video mode */
  int86(0x10, &r, &r);
  current_mode = r.h.al;
  ```
- **Small functions.** One screen, one job. Deep nesting and 300-line functions
  are hard to fit in a 64 KB code segment mentally *and* literally; keeping
  functions small also keeps the small-model tools inside their single code
  segment.
- **No "maybe" `//` comments, no dead code.** If a line is disabled, delete it;
  git remembers. Speculative `//`-commented experiments do not compile under
  `-za` anyway.
- **Watch every segment limit.** Large static tables, big `switch` jump tables,
  and long string pools all consume segment space. When a link fails with a
  segment-overflow or a "group exceeds 64K" error, split the module or move the
  offending table into its own segment — do not silently switch to huge model.

---

## 14.8 Assembly usage guidelines

Assembly is the exception, not the rule. Write it only where C in real mode
cannot express the intent or cannot meet the timing:

- **CPU detection.** Distinguishing 8086 / 286 / 386SX / 386DX / 486 requires
  flag-bit and instruction tests that only assembly performs safely.
- **Tight video / timing loops.** A blit or a scanline effect that must hit a
  cycle budget on a 16-bit-bus 386SX may need hand-written asm.
- **Interrupt service routines.** A custom timer or keyboard ISR needs a precise
  prologue/epilogue (register save, `iret`, EOI to the PIC) that is cleaner and
  safer in asm.

Everything else stays in C.

### NASM syntax and C interop

Write NASM (Intel syntax), assemble to **OMF** so `wlink` can consume it:

```sh
nasm -f obj -o build/cpudet.obj src/common/cpudet.asm
```

To be callable from Watcom C, the routine must match the model's segment/group
names and the C symbol decoration. In small model, C code lives in segment
`_TEXT` (class `CODE`) and a C-visible function `cpu_detect` is emitted as the
public symbol `cpu_detect_` (Watcom appends a trailing underscore). Example
leaf routine returning a CPU class code in `AX`:

```asm
; src/common/cpudet.asm  -  16-bit real-mode CPU class detector
        bits 16

        segment _TEXT public align=2 class=CODE use16

        global  cpu_detect_        ; C:  int cpu_detect(void);
cpu_detect_:
        ; ... FLAGS-bit and opcode probes set AX to 0=8086 1=286 3=386 ...
        ret

        segment _DATA public align=2 class=DATA use16
```

Because Open Watcom C defaults to a **register-based calling convention** (not
`cdecl`), the cleanest way to bind an asm routine is to describe its interface
with `#pragma aux` on the C side, so the compiler knows exactly which registers
carry arguments and results:

```c
/* Tell Watcom: cpu_detect takes nothing, returns result in AX,
   and modifies AX/BX/CX/DX. */
extern int cpu_detect( void );
#pragma aux cpu_detect = "" value [ax] modify [ax bx cx dx];
```

Link the assembled object exactly like a C object — list it in the `wlink`
`file` set or add it to `common.lib` (as the makefile does with `cpudet.obj`):

```sh
wlink system dos name build/HWINFO.EXE &
      file {build/hwinfo.obj build/cpudet.obj} &
      library build/common.lib option quiet
```

Keep asm modules tiny, documented, and C-callable. Do not scatter inline asm
through C files; isolate it in `.asm` units under `src/common` (or the owning
tool) so the C stays portable to Turbo C.

---

## 14.9 Debugging workflow

| Tool | What it is | When to use it |
|---|---|---|
| `wd` / `wdw` | Open Watcom Debugger (character-mode `wd`, windowed `wdw`) | Source-level debugging of Castalia tools; breakpoints, watches, single-step |
| `DEBUG.COM` | FreeDOS `DEBUG` (open replacement) | Quick assembly-level poking: `u` (unassemble), `r` (registers), `d` (dump), `g` (go) |
| serial `printf` | Homemade trace over COM1 | Tracing when the program owns the video screen |
| Bochs / 86Box debugger | Emulator-integrated debuggers | Cold hardware-level bugs: memory breakpoints, port I/O watch, single-stepping the whole machine |

### Open Watcom Debugger (`wd`)

Build a debug variant with symbols and no size squeeze:

```sh
wcl -0 -bt=dos -ms -d2 -wx -za src/hwinfo/hwinfo.c -fe=build/HWINFO.EXE
```

`-d2` embeds full source line and local-variable info. Run `wd HWINFO.EXE`
inside DOSBox-X (or on the target). `wd` supports **remote debugging** over a
null-modem cable: run the serial debug server on the 386SX and connect `wd`
from the dev box, which lets you source-debug on real hardware without a
monitor stealing the screen.

### DEBUG.COM

For a one-off "what is at this address / what opcode is here" question,
FreeDOS `DEBUG.COM` is faster than spinning up `wd`:

```
DEBUG HWINFO.EXE
-u cs:0100        unassemble
-d ds:0000        dump data
-r                show registers
-g                run
-q                quit
```

### printf-to-serial

When a tool has taken over the VGA text screen (menus, the launcher), the most
reliable trace channel is the serial port. Send trace text out COM1 (I/O base
`0x3F8`) via BIOS INT 14h or a tiny UART writer, and read it on the dev box with
a terminal over a null-modem cable:

```c
/* Minimal COM1 trace: BIOS INT 14h, function 01h (send char) */
#include <i86.h>
static void ser_putc(char c) {
    union REGS r;
    r.h.ah = 0x01;          /* send character            */
    r.h.al = (unsigned char)c;
    r.x.dx = 0x0000;        /* COM1                      */
    int86(0x14, &r, &r);
}
void ser_puts(const char *s) { while (*s) ser_putc(*s++); }
```

This survives full-screen UIs and works on real hardware where no on-screen
debugger is welcome.

### Emulator debuggers (Bochs / 86Box)

For bugs that live below your code — a wrong port write, a stray far pointer, a
corrupted interrupt vector — use an emulator with an integrated debugger. Bochs
(built with its internal debugger) and 86Box both offer machine-level
breakpoints, memory/port watchpoints, and single-step over the entire CPU
state. This is where you catch the "it only breaks on the 386SX" class of bug
before touching real hardware.

---

## 14.10 Testing workflow

Testing is a three-stage ladder from fastest to most authoritative. Every change
climbs it in order.

| Stage | Environment | Purpose | Fidelity |
|---|---|---|---|
| 1. Iterate | **DOSBox-X** | Fast build/run loop, functional correctness | Approximate CPU/timing |
| 2. Validate hardware | **86Box / PCem** | Accurate 386SX timing, ISA, VGA, Sound Blaster | Cycle-close, real chipset behavior |
| 3. Certify | **Real 386SX hardware** | Final sign-off before release | Ground truth |

### Stage 1 — DOSBox-X (fast iteration)

DOSBox-X is the everyday loop: build on the host, mount `build/`, run the tool,
repeat in seconds. Configure it to *approximate* the target so gross mistakes
surface early:

```ini
[cpu]
core     = normal
cputype  = 386
cycles   = fixed 3000      ; roughly a slow 386-class machine
[dosbox]
machine  = svga_s3         ; VGA text + SVGA; or vgaonly for strict VGA
```

DOSBox-X is excellent for logic, menus, INI parsing, and INT 10h/16h behavior.
It is **not** authoritative for timing or exact hardware quirks — never certify
a timing-sensitive change here.

### Stage 2 — 86Box / PCem (accurate 386SX)

When correctness looks good, move to 86Box (or PCem) configured as an actual
386SX machine: a real 386SX CPU model at a real clock (16 / 20 / 25 MHz), a
period ISA VGA card, and an emulated Sound Blaster matching the
`SET BLASTER=A220 I5 D1 H5 T4`-style profiles. This tier reproduces the 16-bit
memory bus, realistic wait states, and true DMA/IRQ behavior, so it is where you
verify:

- CPU detection returns "386SX" and memory-profile switching behaves,
- video timing and any tight loops feel right at real clock speeds,
- sound init on emulated AdLib / SB actually plays.

If it is right in 86Box at 386SX settings, it is almost certainly right on metal.

### Stage 3 — Real 386SX hardware (final validation)

The last stop is a physical 386SX. Nothing else certifies a release. Get the
built binaries onto the machine by whichever transport the target supports:

- **CompactFlash + CF-to-IDE adapter.** Write the disk image (or copy files) to
  a CF card in a USB CF reader on the dev box, then boot the 386SX from it as an
  IDE drive. The fastest, most reliable path for a machine with a CF adapter.
- **Floppy.** Copy the `.EXE` files onto a 1.44 MB (or 720 KB) floppy and carry
  it over. Slow but universal; good for a single tool.
- **Null-modem serial.** Transfer over a COM-to-COM cable (e.g. a Kermit or
  serial file-copy utility on both ends) when the machine has no removable media
  you can read on the modern host. This doubles as your `wd` remote-debug and
  serial-trace channel.

On the target, run the full boot menu (CLEAN / XMS / EMS / …), launch a real
game through `LAUNCH.EXE`, switch profiles with `MEMPROF.EXE`, and confirm free
conventional memory matches the profile's promise. Only after a clean pass on
real 386SX hardware does a build become a release candidate.

---

*End of Section 14 — Developer Toolchain.*
