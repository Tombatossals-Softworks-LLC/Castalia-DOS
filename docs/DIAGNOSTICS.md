# Section 10 — Hardware Diagnostics (`HWINFO.EXE`)

CASTALIA DOS 386SX Edition · Technical Bible · Version codename target 0.5
"Morella" and later. This section specifies the design of `HWINFO.EXE`, the
Castalia hardware diagnostics tool that reports what the machine actually is and
what the running boot profile has enabled.

`HWINFO.EXE` lives in `C:\CASTALIA\BIN` (on the PATH). It is launched
automatically by `AUTOEXEC.BAT` when the boot menu selects the **DIAG** profile
(see Section 3, boot profiles), and can be run by hand at any prompt. It is an
original Castalia MIT-licensed tool; it links no FreeDOS/GPL code and talks to
drivers only through documented DOS/BIOS interrupts.

---

## 10.1 Purpose and constraints

`HWINFO.EXE` answers one question calmly and honestly: **"What is this machine,
and what did this boot give me?"** It is a first-run and troubleshooting tool.
When a game will not start, the operator runs `HWINFO`, reads the panel, and
knows whether the problem is a missing driver, the wrong memory profile, or a
genuine hardware limit.

Design constraints (non-negotiable):

| Constraint            | Rule                                                        |
|-----------------------|-------------------------------------------------------------|
| Video                 | Pure VGA 80x25 text mode, 16 colors, CP437. No graphics.    |
| Footprint             | Small/large model, 16-bit real mode, well under 64 KB.      |
| Residency             | **No TSR, no resident code.** Runs, prints, exits cleanly.  |
| Interrupt hygiene     | Saves/restores every vector it must touch. Hooks nothing.   |
| Allocation            | Fixed static buffers only. No `malloc` in the hot path.     |
| CPU floor             | Must run on a bare 8086 so it can *report* an 8086.          |
| Determinism           | Read-only probes preferred. No writes to hardware ports     |
|                       | unless the probe is explicitly opt-in (`/PROBE`, see 10.9). |
| Honesty               | Reports "configured" vs "detected" separately. Never claims |
|                       | knowledge DOS cannot reliably provide (see 10.8).           |

Because `HWINFO` may run under the **SAFE** profile (no drivers) or the **CLEAN**
profile (HIMEMX only), every probe must degrade gracefully: absence of a driver
is a normal, reportable result, never a crash.

**The core honesty principle.** DOS and the PC BIOS were never designed to
enumerate hardware. They report *what a driver has claimed* far more reliably
than *what silicon is present*. `HWINFO` therefore separates two columns
everywhere it matters: **Configured** (what an environment variable or driver
declares) and **Detected** (what a safe probe confirms). It never promotes the
first into the second.

---

## 10.2 Detection matrix (summary)

Reliability legend: **A** = authoritative / reliable; **B** = reliable when the
relevant driver is loaded; **C** = best-effort, may be wrong or "unknown".

| # | Item                | Primary method                              | Reliab. |
|---|---------------------|---------------------------------------------|---------|
| 1 | CPU class           | FLAGS bits 12-15, EFLAGS AC/ID, `CPUID`     | A       |
| 2 | FPU present         | `FNINIT` + `FNSTSW`/`FNSTCW` probe          | A       |
| 3 | Conventional RAM    | `INT 12h`                                    | A       |
| 4 | Extended RAM        | XMS query `INT 2Fh 4310h` → AH=08h; `15h/88h`| B / C  |
| 5 | EMS present         | Open `EMMXXXX0` device; `INT 67h`           | B       |
| 6 | XMS present         | `INT 2Fh AX=4300h`                           | A       |
| 7 | VGA present         | `INT 10h AX=1A00h`; fallback `INT 10h 12h`  | A       |
| 8 | Mouse driver        | `INT 33h AX=0000h` (after vector check)     | B       |
| 9 | CD-ROM (MSCDEX)     | `INT 2Fh AX=1500h` install check            | B       |
|10 | Sound configuration | Parse `BLASTER` env; optional DSP probe     | C       |
|11 | DOS version         | `INT 21h AH=30h`; true ver `AH=33h AL=06h`  | A       |
|12 | Free disk space     | `INT 21h AH=36h`                             | A       |
|13 | Boot profile        | Ask the kernel (`INT 2Fh CA01h`); `%CASTPROFILE%` | A   |

Nothing in this table claims exact CPU MHz, exact sound-card model, exact IRQ/DMA
by silicon, or CompactFlash-vs-hard-disk. Those are covered in 10.8 as things
DOS cannot reliably detect.

---

## 10.3 Per-item detection methods

### 1. CPU class — 8086 / 286 / 386 / 486 / Pentium

Real-mode CPU stepping is detected by exploiting where each generation stopped
honoring writes to the FLAGS/EFLAGS register. This is the classic Intel AP-485
staircase:

1. **8086/8088 vs 286+** — In real mode the 8086 keeps FLAGS bits **12-15**
   permanently set to 1. Push a FLAGS image with bits 12-15 *cleared*, `POPF`,
   `PUSHF`, read back: if bits 12-15 are still 1, it is an 8086/8088.
2. **286 vs 386+** — In real mode the 286 keeps FLAGS bits **12-15** permanently
   *0*. Push a FLAGS image with bits 12-15 *set*, `POPF`, `PUSHF`, read back: if
   they are still 0, it is a 286.
3. **386 vs 486+** — Only the 32-bit EFLAGS **AC** bit (bit 18, Alignment Check)
   is toggleable on a 486 and above, not on a 386. Flip bit 18 with
   `PUSHFD/POPFD`; if it sticks, it is a 486 or later.
4. **486 vs Pentium+** — The EFLAGS **ID** bit (bit 21) is toggleable only on
   CPUs that implement `CPUID`. If bit 21 flips, `CPUID` is available; execute
   `CPUID` with EAX=1 to read family/model and confirm Pentium-class or later.

Reliability **A**. Vendor sub-typing (Intel vs AMD vs Cyrix) via the `CPUID`
vendor string is reported only when `CPUID` is present; on a plain 386SX we
report "80386" and stop — which is exactly correct for the flagship target.

### 2. FPU (math coprocessor) presence

On the 386SX/DX the FPU (387SX/387) is a *separate, optional* chip; on the 486DX
and Pentium it is integral (a 486SX has it fused off). Detection uses the
non-waiting coprocessor instructions so the probe cannot hang on a machine with
no coprocessor:

```
FNINIT                  ; no-wait init; harmless if no FPU
FNSTCW  [cw]            ; store control word
MOV     AX,[cw]
AND     AX,103Fh
CMP     AX,003Fh        ; after FNINIT control word = 037Fh
JNE     no_fpu          ; masked bits do not match -> no FPU
; corroborate with the status word:
FNSTSW  [sw]            ; after FNINIT low byte must be 00h
CMP     byte ptr [sw],0
JNE     no_fpu
```

Using `FNINIT`/`FNSTCW`/`FNSTSW` (the `FNxxx` no-wait forms) avoids the `WAIT`
prefix that would deadlock on an 8087/287-less machine. Reliability **A**.

### 3. Conventional memory — `INT 12h`

`INT 12h` returns **AX = KB of contiguous conventional memory** (normally 640).
It simply reads the BIOS word at `0040:0013`. A value below 640 usually means an
EBDA or a memory-manager reservation. Reliability **A**. This is the number that
matters to real-mode games, so `HWINFO` prints both this figure and the current
largest free block (from DOS, `INT 21h AH=48h BX=FFFFh` best-fit probe).

### 4. Extended memory (above 1 MB)

Two paths, with a deliberate order of trust:

- **Preferred — XMS driver query.** If HIMEMX is loaded, get the XMS entry point
  (10.3 item 6), then call the far entry with **AH=08h** ("Query Free Extended
  Memory"): returns AX = largest free block (KB), DX = total free (KB). This is
  what games can actually obtain, so it is the honest number. Reliability **B**
  (needs the driver).
- **Fallback — `INT 15h AH=88h`.** Returns AX = KB above 1 MB. **Unreliable when
  an XMS manager is loaded**: HIMEMX commonly hooks or zeroes this call, so a
  result of 0 here does *not* mean "no extended memory." Capped at 64 MB
  (16-bit KB). On raw hardware without a manager it is roughly right.
  `INT 15h AX=E801h` extends the range past 64 MB where the BIOS supports it.
  Reliability **C** in the presence of a manager.

`HWINFO` prints the XMS-derived figure as **Detected** and, if no XMS manager is
present, falls back to `88h`/`E801h` labelled "(BIOS, no XMS)".

### 5. EMS (Expanded Memory) presence — `EMMXXXX0` / `INT 67h`

The safe, recommended method opens the EMS **character device** by its fixed
name rather than trusting the raw `INT 67h` vector:

1. `INT 21h AX=3D00h`, DS:DX → the ASCIIZ string `"EMMXXXX0"`. On success we get
   a handle.
2. `INT 21h AX=4400h` (IOCTL get-device-info) on the handle; confirm bit 7 of DX
   is set (it is a **device**, not a same-named file on disk).
3. `INT 21h AH=3Eh` close the handle.
4. Only then call `INT 67h AH=46h` (Get EMM version) and `AH=42h` (Get free/total
   pages; each EMS page = 16 KB) and `AH=40h` (status) to fill in the numbers.

The older method — read the `INT 67h` vector (`INT 21h AX=3567h`), then compare
the 8 bytes at `ES:000Ah` (the device-header name field) against `"EMMXXXX0"` —
is also acceptable and is used as a cross-check. Reliability **B**: it correctly
reports "EMS not available" under the CLEAN/XMS/SAFE profiles (which load no EMS
page frame) and reports page counts under EMS/CDROM/WIN3X.

### 6. XMS presence — `INT 2Fh AX=4300h`

The XMS install check is a DOS multiplex call and is fully reliable:

```
MOV  AX,4300h
INT  2Fh
CMP  AL,80h          ; AL=80h  -> XMS driver present
```

If present, `INT 2Fh AX=4310h` returns the driver's **far entry point** in
ES:BX. Store it; all real XMS work (AH=00h version, AH=08h free query, AH=09h
allocate) is done by `CALL`ing that far address, not by another interrupt.
Reliability **A**.

### 7. VGA presence — `INT 10h AX=1A00h`

The VGA/MCGA BIOS "Get Display Combination Code" call is the cleanest adapter
probe:

```
MOV  AX,1A00h
INT  10h
CMP  AL,1Ah          ; AL=1Ah on return -> call supported -> VGA/MCGA BIOS
; BL = active display code: 08h VGA color, 07h VGA mono,
;      04h/05h EGA, 02h/03h CGA, 01h MDA, 0Ah/0Bh/0Ch PS/2 analog...
```

If `AL != 1Ah` the machine predates VGA; fall back to `INT 10h AH=12h BL=10h`
(Get EGA info): if BL changes from 10h, an EGA/VGA is present, else CGA/MDA.
`INT 10h AH=0Fh` (Get Video Mode) then confirms the current text mode and column
count. Reliability **A** for "VGA-class present". We do **not** claim the exact
chipset (Trident/Cirrus/ET4000) or VRAM size — that needs a VESA VBE BIOS
(`INT 10h AX=4F00h`), which we query only if present and otherwise report
"unknown". See 10.8.

### 8. Mouse driver presence — `INT 33h AX=0000h`

The mouse API is only safe to call once we know something answers it. Calling
`INT 33h` when the vector is `0000:0000` would execute whatever garbage sits at
address 0. So:

1. Read the `INT 33h` vector (`INT 21h AX=3533h`). If segment:offset is
   `0000:0000`, there is no driver — stop.
2. If the first byte at the vector target is `CFh` (`IRET`), some BIOSes stub the
   vector; treat as no driver.
3. Otherwise `INT 33h AX=0000h` (reset & status): **AX=FFFFh** means a driver is
   installed, and **BX = button count**. AX=0 means no driver.

Reliability **B**: this detects the *driver* (CTMOUSE, under the XMS/EMS/WIN3X
profiles), not the physical mouse. A serial mouse with no driver loaded is,
correctly, reported as "no mouse driver".

### 9. CD-ROM (MSCDEX/SHSUCDX) presence — `INT 2Fh AX=1500h`

The CD redirector install check works identically for Microsoft MSCDEX and for
Castalia's bundled **SHSUCDX** replacement (both answer the `2Fh/15xx` API):

```
XOR  BX,BX
MOV  AX,1500h
INT  2Fh
; BX = number of CD-ROM drive letters (0 -> not installed)
; CX = first drive number (0=A:, 1=B:, 2=C: ...)
```

If `BX != 0`, `INT 2Fh AX=1501h` returns the CDROM driver-header list. Reliability
**B**: present only under the **CDROM** profile (which loads `UIDE.SYS` +
`SHSUCDX`). We report the redirector and the assigned drive letter, not the ATAPI
model string (which would require an `INT 2Fh 150Ch`/device passthrough and is
left "unknown" by default).

### 10. Sound configuration — parse `BLASTER`

`HWINFO` reads the environment variable `BLASTER` (via `getenv` or by scanning
the program's environment segment) and parses the canonical Castalia form
`SET BLASTER=A220 I5 D1 H5 T4`:

| Token | Meaning              | Example |
|-------|----------------------|---------|
| `Axxx`| I/O base port (hex)  | `A220`  |
| `Ix`  | IRQ                  | `I5`    |
| `Dx`  | 8-bit DMA channel    | `D1`    |
| `Hx`  | 16-bit DMA channel   | `H5`    |
| `Tx`  | card type code       | `T4`    |
| `Pxxx`| MPU-401 base (opt.)  | `P330`  |

This is **Configured**, not **Detected** (reliability **C**). The `BLASTER`
string is whatever `SETSOUND.EXE`/`AUTOEXEC` declared; it is not proof any card
answers at that port. An optional, explicit DSP probe (`/PROBE`, 10.9) may reset
the Sound Blaster DSP (write `00h` to port `base+6`, clear, read `base+0Eh` for
`AAh`) and test the AdLib FM timers at port `388h`, but that is opt-in because it
writes to hardware. Card *model* is never asserted — see 10.8.

### 11. DOS version — `INT 21h AH=30h`

`INT 21h AH=30h` returns **AL=major, AH=minor**, plus OEM byte in BH and a 24-bit
serial in BL:CX. For DOS 5+ the *true* (un-SETVER'd) version comes from
`INT 21h AX=3306h` (BL=major, BH=minor). Reliability **A** for the raw values,
with an important honesty note printed by `HWINFO`:

> The FreeDOS kernel reports **7.x** (e.g. 7.10). Castalia presents
> **"6.22-compatible"** behavior and uses `SETVER` per-program for titles that
> demand a specific version. There is **no global 6.22 spoof.** `HWINFO` shows
> the real kernel report *and* the "6.22-compatible" posture, so nobody is
> misled. `SETVER` changes what a *listed program* sees, not what `HWINFO` reads
> for itself (unless `HWINFO` were itself in the SETVER list, which it is not).

### 12. Free disk space — `INT 21h AH=36h`

```
MOV  DL,drive          ; 0 = default, 1 = A:, 2 = B:, 3 = C: ...
MOV  AH,36h
INT  21h
; AX = sectors per cluster (FFFFh -> invalid drive)
; BX = available clusters
; CX = bytes per sector
; DX = total clusters
; free bytes = (unsigned long)AX * BX * CX
```

Reliability **A** on FAT12/FAT16 up to the classic ~2 GB limit. The multiply
**must** be done in 32-bit (`unsigned long`) or it overflows silently — a real
bug source. Castalia is FAT12/FAT16 only, so the FAT32 call (`INT 21h AX=7303h`)
is out of scope and not used. `HWINFO` reports total, free, and cluster size for
the current drive and for `C:`.

### 13. Boot profile — the kernel first, `%CASTPROFILE%` second

The CONFIG.SYS boot menu makes the kernel export **`%CONFIG%`**, and it is the
menu **digit** `"1"`…`"8"`, not a name — the FreeDOS kernel has no named
`[blocks]` (see [`BOOT.md`](BOOT.md)). Castalia's `AUTOEXEC.BAT` maps that digit
to **`%CASTPROFILE%`** — one of `CLEAN`, `XMS`, `EMS`, `CDROM`, `WIN3X`, `DIAG`,
`SAFE`, `PROMPT` (Section 3).

The authority, though, is the kernel itself: the `n?CASTALIA=n` directive stores
the profile in a resident kernel byte, readable with `INT 2Fh AX=0CA01h`
(see [`KERNEL.md`](KERNEL.md)). Tools ask the kernel first and fall back to
`getenv("CASTPROFILE")` only on a stock kernel, which is why the profile shown
stays correct even if a batch file clobbers the variable. Reliability **A**.
If neither source answers — e.g. the user booted a custom CONFIG.SYS with no
menu and no `CASTALIA=` line — the report reads "profile: (custom / unknown)"
rather than guessing.

---

## 10.4 Overall program flow (pseudocode)

```text
program HWINFO:
    parse_command_line()          # /PROBE, /NOCOLOR, /LOG=file, /?

    save_video_state()            # current mode/page from INT 10h AH=0Fh
    set_text_mode_80x25()         # INT 10h AX=0003h only if not already 80x25

    info = zeroed HWINFO record   # one fixed static struct, no malloc

    # --- environment / DOS layer (cheap, always safe) ---
    info.profile   = kernel_profile()            # 13  INT 2Fh AX=0CA01h
    if not info.profile:                         #     stock kernel fallback
        info.profile = getenv("CASTPROFILE")
    info.dos       = dos_version()               # 11  INT 21h 30h / 3306h
    info.blaster   = parse_blaster(getenv("BLASTER"))   # 10

    # --- CPU / FPU (instruction probes, no I/O) ---
    info.cpu = detect_cpu_class()                # 1
    info.fpu = detect_fpu()                      # 2

    # --- memory ---
    info.conv = int12_conventional_kb()          # 3
    info.xms  = xms_present()                     # 6  INT 2Fh 4300h
    if info.xms.present:
        info.xms.entry = xms_entry_point()       #    INT 2Fh 4310h
        info.xms.free  = xms_query_free(entry)   # 4  AH=08h
    else:
        info.ext_bios  = int15_88_extended_kb()  # 4  fallback, label "no XMS"
    info.ems  = ems_present()                    # 5  EMMXXXX0 / INT 67h

    # --- devices (each guards its own vector first) ---
    info.vga    = vga_present()                  # 7  INT 10h 1A00h
    info.mouse  = mouse_driver_present()         # 8  INT 33h 0000h (guarded)
    info.cdrom  = mscdex_present()               # 9  INT 2Fh 1500h

    # --- disk ---
    info.diskCur = disk_free(default_drive)      # 12 INT 21h 36h
    info.diskC   = disk_free('C')

    if command_line.probe:                       # opt-in hardware writes
        info.sbProbe = probe_sound_blaster(info.blaster.port)

    render_results_screen(info)                  # 10.7 layout
    if command_line.log: write_report(info, logfile)

    wait_for_key()
    restore_video_state()                        # leave the screen as found
    return 0                                     # never resident, always exit
```

Ordering rationale: the cheap, always-safe environment/DOS reads run first so a
severely broken machine (SAFE profile) still shows *something*; the guarded
device probes run last; the only hardware-writing probe is gated behind
`/PROBE`.

---

## 10.5 Trickier probes (pseudocode)

### CPU class

```text
function detect_cpu_class() -> code:     # 0=8086 2=286 3=386 4=486 5=Pentium+
    # step 1: 8086 keeps FLAGS bits 12..15 = 1
    f = read_flags()
    write_flags(f AND 0x0FFF)            # try to clear 12..15
    if (read_flags() AND 0xF000) == 0xF000:
        return 0                         # 8086/8088

    # step 2: 286 keeps FLAGS bits 12..15 = 0 (real mode)
    write_flags(f OR 0xF000)             # try to set 12..15
    if (read_flags() AND 0xF000) == 0:
        return 2                         # 80286

    # step 3+: 386+, switch to 32-bit EFLAGS
    e = read_eflags()
    if not toggles(e, bit 18 AC):        # AC fixed -> 386
        return 3
    if not toggles(e, bit 21 ID):        # ID fixed -> plain 486 (no CPUID)
        return 4
    cpuid(eax=1)                         # ID toggles -> CPUID exists
    family = (eax >> 8) AND 0x0F
    return (family >= 5) ? 5 : 4         # family 5 = Pentium class

helper toggles(e, bit):
    write_eflags(e XOR bit)
    changed = (read_eflags() XOR e) AND bit
    write_eflags(e)                      # restore
    return changed != 0
```

### FPU presence

```text
function detect_fpu() -> bool:
    fninit()                             # no-wait init; safe w/o coprocessor
    cw = fnstcw()                        # control word
    if (cw AND 0x103F) != 0x003F:        # FNINIT leaves control word = 0x037F
        return false
    sw = fnstsw()                        # status word
    if (sw AND 0x00FF) != 0:             # low byte must be 0 after FNINIT
        return false
    return true
```

### EMS presence

```text
function ems_present() -> {present, pages_total, pages_free, version}:
    h = dos_open("EMMXXXX0", read_only)          # INT 21h AX=3D00h
    if open_failed(h):
        return {present:false}
    info = dos_ioctl_get_device_info(h)          # INT 21h AX=4400h
    dos_close(h)                                 # INT 21h AH=3Eh
    if (info AND 0x0080) == 0:                   # bit 7 clear -> it was a FILE
        return {present:false}                   # EMMXXXX0 was a file, not EMM
    # cross-check the INT 67h device header name
    seg = int67_vector_segment()                 # INT 21h AX=3567h
    if bytes_at(seg:0x000A, 8) != "EMMXXXX0":
        return {present:false}
    ver   = int67(AH=0x46)                        # get version
    total,free = int67(AH=0x42)                   # pages; 16 KB each
    return {present:true, total, free, ver}
```

### MSCDEX / SHSUCDX presence

```text
function mscdex_present() -> {present, num_drives, first_letter}:
    bx = 0
    ax = 0x1500
    int2f(ax, bx)                                # INT 2Fh AX=1500h
    if bx == 0:
        return {present:false}                   # no CD redirector loaded
    num   = bx                                   # count of CD drive letters
    first = 'A' + cx                             # cx = 0-based first drive
    return {present:true, num, first}
```

---

## 10.6 C + inline-assembly strategy (Open Watcom / Turbo C)

`HWINFO` is C89, 16-bit real mode, portable between **Open Watcom** (`wcc`/`wcl`,
the primary toolchain) and **Turbo C** (secondary). The build line matches the
project standard:

```
wcl -0 -bt=dos -ml -os hwinfo.c dosprobe.c cpuasm.asm
```

`-0` emits 8086 code so the binary itself runs on the 8086 it may need to report.

### The `int86` vs `int386` gotcha

Both compilers expose `int86()`/`int86x()` with `union REGS` for **real-mode**
interrupts, and that is all `HWINFO` uses. Open Watcom *additionally* offers
`int386()`/`int386x()` for 32-bit flat/protected targets — **we do not use it**,
because Castalia is a 16-bit real-mode target. The one real incompatibility is
the *shape* of `union REGS`:

| Compiler     | 16-bit word regs   | Header       | Extended (32-bit)     |
|--------------|--------------------|--------------|-----------------------|
| Turbo C      | `regs.x.ax`        | `<dos.h>`    | n/a (16-bit only)     |
| Open Watcom  | `regs.w.ax`        | `<i86.h>`    | `regs.x.eax` (int386) |

Watcom's `union REGS` names the 16-bit sub-struct `w` and reserves `x` for the
32-bit `DWORDREGS`; Turbo C names the 16-bit sub-struct `x`. Bridge it with one
macro and both compile from the same source:

```c
/* portab.h -- one place for the REGS shape difference */
#ifdef __WATCOMC__
  #include <i86.h>
  #define WR(r)  ((r).w)      /* Open Watcom: 16-bit regs live in .w */
#else
  #include <dos.h>
  #define WR(r)  ((r).x)      /* Turbo C / Borland: 16-bit regs in .x */
#endif
/* byte and segment regs (.h / struct SREGS) are spelled the same in both */
```

### Portable interrupt probes (real, compilable style)

```c
#include "portab.h"
#include <string.h>

/* --- item 3: conventional memory, INT 12h --------------------------- */
unsigned conventional_kb(void)
{
    union REGS r;
    int86(0x12, &r, &r);
    return WR(r).ax;                 /* AX = KB (usually 640) */
}

/* --- item 11: DOS version, INT 21h AH=30h --------------------------- */
void dos_version(unsigned char *major, unsigned char *minor)
{
    union REGS r;
    r.h.ah = 0x30;
    int86(0x21, &r, &r);
    *major = r.h.al;                 /* AL = major */
    *minor = r.h.ah;                 /* AH = minor */
}

/* --- item 6: XMS install check, INT 2Fh AX=4300h -------------------- */
int xms_present(void)
{
    union REGS r;
    WR(r).ax = 0x4300;
    int86(0x2F, &r, &r);
    return (r.h.al == 0x80);         /* AL=80h -> XMS driver present */
}

/* --- item 9: MSCDEX / SHSUCDX install check, INT 2Fh AX=1500h ------- */
int mscdex_present(unsigned *num_drives, unsigned *first_drive)
{
    union REGS r;
    WR(r).ax = 0x1500;
    WR(r).bx = 0x0000;
    int86(0x2F, &r, &r);
    if (WR(r).bx == 0) return 0;     /* not installed */
    *num_drives  = WR(r).bx;
    *first_drive = WR(r).cx;         /* 0 = A: */
    return 1;
}

/* --- item 12: free disk space, INT 21h AH=36h ----------------------- */
unsigned long disk_free_bytes(unsigned char drive /*0=cur,1=A:*/)
{
    union REGS r;
    r.h.ah = 0x36;
    r.h.dl = drive;
    int86(0x21, &r, &r);
    if (WR(r).ax == 0xFFFF) return 0UL;             /* invalid drive */
    /* 32-bit math is mandatory or this overflows: */
    return (unsigned long)WR(r).ax   /* sectors/cluster */
         * (unsigned long)WR(r).bx   /* free clusters   */
         * (unsigned long)WR(r).cx;  /* bytes/sector    */
}

/* --- item 8: mouse driver, guarded INT 33h AX=0000h ----------------- */
int mouse_driver_present(unsigned *buttons)
{
    union REGS  r;
    struct SREGS s;
    unsigned seg, off;

    /* read the INT 33h vector via INT 21h AX=3533h before touching it */
    r.h.ah = 0x35; r.h.al = 0x33;
    int86x(0x21, &r, &r, &s);
    seg = s.es; off = WR(r).bx;
    if (seg == 0 && off == 0) return 0;             /* no vector -> no driver */
    if (*(unsigned char far *)MK_FP(seg, off) == 0xCF) return 0; /* IRET stub */

    WR(r).ax = 0x0000;
    int86(0x33, &r, &r);
    if (WR(r).ax != 0xFFFF) return 0;               /* no driver */
    *buttons = WR(r).bx;
    return 1;
}
```

`MK_FP` and the `far` keyword exist in both Turbo C and Open Watcom (16-bit).
Every probe reads its own state and touches no vector without checking it first.

### Compact CPU-detection routine (separate `.asm`, both toolchains link it)

Kept in `cpuasm.asm` and assembled by WASM (Watcom) or TASM (Borland). The
`.386` directive lets the 386+ section assemble while the segment stays 16-bit,
so the 8086/286 tests still execute on an 8086.

```asm
        .386                        ; allow 32-bit instrs; segment stays 16-bit
        .model  large
        .code
        public  cpu_class_          ; leading/trailing '_' suits both linkers
;--------------------------------------------------------------------
; unsigned cpu_class(void);
;   0=8086/88  2=80286  3=80386  4=80486  5=Pentium+ (CPUID)
;--------------------------------------------------------------------
cpu_class_ proc
        push    bp
        pushf                       ; save caller FLAGS to restore on exit

        ; --- 8086 keeps FLAGS bits 12..15 = 1 -----------------------
        pushf
        pop     ax
        and     ax, 0FFFh           ; try to clear bits 12..15
        push    ax
        popf
        pushf
        pop     ax
        and     ax, 0F000h
        cmp     ax, 0F000h
        je      c8086               ; stuck at 1 -> 8086/8088

        ; --- 286 keeps FLAGS bits 12..15 = 0 (real mode) ------------
        pushf
        pop     ax
        or      ax, 0F000h          ; try to set bits 12..15
        push    ax
        popf
        pushf
        pop     ax
        and     ax, 0F000h
        jz      c286                ; stuck at 0 -> 80286

        ; --- 386+: AC bit (18) toggles only on 486+ -----------------
        pushfd
        pop     eax
        mov     ecx, eax            ; keep original EFLAGS
        xor     eax, 40000h         ; flip AC
        push    eax
        popfd
        pushfd
        pop     eax
        xor     eax, ecx
        and     eax, 40000h
        jz      c386                ; AC fixed -> 80386

        ; --- ID bit (21) toggles only if CPUID exists ---------------
        mov     eax, ecx
        xor     eax, 200000h        ; flip ID
        push    eax
        popfd
        pushfd
        pop     eax
        xor     eax, ecx
        and     eax, 200000h
        jz      c486                ; ID fixed -> plain 80486, no CPUID

        mov     eax, 1              ; CPUID leaf 1
        cpuid
        shr     eax, 8
        and     eax, 0Fh            ; family
        cmp     eax, 5
        jae     c586
        mov     ax, 4               ; family<5 but CPUID -> late 486
        jmp     cdone
c586:   mov     ax, 5
        jmp     cdone
c486:   mov     ax, 4
        jmp     cdone
c386:   mov     ax, 3
        jmp     cdone
c286:   mov     ax, 2
        jmp     cdone
c8086:  xor     ax, ax
cdone:  popf                        ; restore caller FLAGS
        pop     bp
        ret
cpu_class_ endp
        end
```

Watcom users who prefer no external assembler can express the same routine as a
`#pragma aux` naked function; the `.asm` form above is kept because it assembles
identically under WASM and TASM and keeps the C files clean. The FPU probe is
handled the same way (a tiny `fpu_present_` in `cpuasm.asm` running
`FNINIT`/`FNSTCW`/`FNSTSW`), since `FN*` opcodes are awkward to spell portably in
inline C.

---

## 10.7 Results screen (80x25 text-mode mockup)

Castalia palette: blue background (attr 1), light-gray/white panels and frames,
amber/yellow (14) for the title and headings, black-on-amber for the active
profile chip, green/red glyphs for present/absent. The mockup below uses ASCII
box characters; the shipping build draws CP437 single/double line characters
(0xC9/0xCD/0xBB frame, 0xB3/0xC4 rules).

```text
##============================================================================##
||  CASTALIA DOS   HWINFO.EXE   Hardware Diagnostics        [ PROFILE: XMS ]  ||
##============================================================================##
|| SYSTEM                                | MEMORY                             ||
||   CPU .......... Intel 80386SX        |   Conventional ...... 640 KB       ||
||   FPU .......... 80387SX  (present)   |   Free (largest) .... 631 KB       ||
||   Speed ........ not measured         |   XMS driver ........ HIMEMX       ||
||   DOS report ... 7.10 kernel          |   XMS free .......... 15360 KB     ||
||   Compatibility  6.22-compatible      |   EMS page frame .... none (XMS)   ||
||                                       |   EMS free .......... n/a          ||
||---------------------------------------+-----------------------------------||
|| VIDEO / INPUT                         | STORAGE                           ||
||   Adapter ...... VGA colour (1A/08)   |   Current drive ..... C:           ||
||   Text mode .... 03h  80x25 16-col    |   C: total .......... 504 MB       ||
||   Mouse driver . CTMOUSE  (2 buttons) |   C: free ........... 271 MB       ||
||   CD-ROM ....... none loaded          |   Cluster size ...... 8 KB         ||
||---------------------------------------+-----------------------------------||
|| SOUND (configured, not probed)                                            ||
||   BLASTER ...... A220 I5 D1 H5 T4     Port 220h  IRQ 5  DMA 1/5  Type SB16 ||
||   Note ......... values are DECLARED by SET BLASTER, not verified          ||
||                  run  HWINFO /PROBE  to test the DSP and AdLib FM          ||
##============================================================================##
||  green = present   red = absent   "configured" = from environment only    ||
||  [F1] Help    [P] Probe sound    [L] Save report    [ESC] Exit            ||
##============================================================================##
```

Notes on the mock: the header profile chip is drawn black-on-amber; the two-column
body uses light-gray rules on the blue field; "present/absent" words are colored
(green 10 / red 12) while the surrounding text stays white/light-gray for the
calm, IBM-era look. Every number that came from an environment variable rather
than a probe sits under a heading that literally says "configured, not probed".

---

## 10.8 Limits and honesty

`HWINFO` refuses to assert what DOS cannot reliably know. This table is part of
the spec, not a disclaimer bolted on afterward.

| Item                          | Detect reliably? | Fallback / what we print          |
|-------------------------------|------------------|-----------------------------------|
| CPU class (86/286/386/486/P5) | Yes (A)          | Show class; vendor only if CPUID  |
| Exact CPU MHz / clock speed   | **No**           | "not measured" (no honest INT for |
|                               |                  | it; BogoMIPS-style loops lie under |
|                               |                  | emulation and cache effects)      |
| FPU present                   | Yes (A)          | `FNINIT` probe; report yes/no     |
| Conventional memory           | Yes (A)          | INT 12h                           |
| Extended memory (with XMS)    | Yes (B)          | XMS AH=08h free query             |
| Extended memory (no manager)  | Partly (C)       | INT 15h 88h/E801h, tag "BIOS,     |
|                               |                  | may be hidden by a manager"       |
| EMS present / free pages      | Yes (B)          | EMMXXXX0 + INT 67h; else "none"   |
| XMS present / free            | Yes (A/B)        | INT 2Fh 4300h + far entry AH=08h  |
| VGA-class present             | Yes (A)          | INT 10h 1A00h                     |
| Exact VGA chipset / VRAM size | **No**           | VESA VBE if a VBE BIOS answers;   |
|                               |                  | else "VGA (chipset unknown)"      |
| Mouse *driver* present        | Yes (B)          | INT 33h 0000h (guarded)           |
| Mouse *hardware* w/o driver   | **No**           | "no mouse driver loaded"          |
| CD *redirector* present       | Yes (B)          | INT 2Fh 1500h                     |
| CD-ROM drive model string     | **No** (default) | "unknown"; only via opt-in ATAPI  |
| Sound card *configured*       | Yes (A)          | parse BLASTER env                 |
| Sound card *model* (exact)    | **No**           | never asserted; show BLASTER type |
|                               |                  | code and DSP-version *if* /PROBE  |
| Exact IRQ/DMA of a card       | **No** w/o probe | show BLASTER-declared I5/D1/H5;   |
|                               |                  | label "declared, not verified"    |
| DOS reported version          | Yes (A)          | INT 21h 30h + 3306h true version  |
| Free disk space (FAT12/16)    | Yes (A)          | INT 21h 36h (32-bit math!)        |
| CompactFlash vs hard disk     | **No**           | both are ATA/IDE to DOS; we print |
|                               |                  | "fixed disk (IDE/ATA)" for both   |
| Boot profile                  | Yes (A)          | INT 2Fh CA01h, else %CASTPROFILE% |

Things `HWINFO` will **never** print as fact: a MHz rating, an exact sound-card
product name, an IRQ/DMA it did not either read from `BLASTER` or confirm by
probe, or a claim that a CompactFlash card is "a CF card" (to the ATA layer and
to DOS it is indistinguishable from a spinning IDE disk). Where the honest answer
is "unknown," the panel says **unknown** — that is a feature.

---

## 10.9 Command line and behavior

```
HWINFO [/PROBE] [/NOCOLOR] [/LOG[=file]] [/?]
```

| Switch      | Effect                                                          |
|-------------|----------------------------------------------------------------|
| (none)      | Run all read-only probes, show the results screen.             |
| `/PROBE`    | Additionally perform *hardware-writing* probes: Sound Blaster  |
|             | DSP reset/read at the BLASTER port and AdLib FM timer test at  |
|             | 388h. Opt-in because it writes to I/O ports.                   |
| `/NOCOLOR`  | Monochrome-safe output (for VGA-mono / LCD / capture).         |
| `/LOG[=f]`  | Also write a plain-text report to `f` (default `HWINFO.TXT` in |
|             | the current directory) for support tickets.                    |
| `/?`        | Usage and exit.                                                |

Exit codes: `0` normal, `1` bad command line, `2` could not set 80x25 text mode
(pre-VGA hardware) — in which case `HWINFO` prints a plain teletype report via
`INT 21h AH=09h` instead of the framed screen and still returns useful data.

On exit `HWINFO` restores the video mode/page it saved on entry and frees any
handle it opened (`EMMXXXX0`). It leaves **no** vector hooked, **no** resident
footprint, and **no** hardware in a changed state (the `/PROBE` DSP reset returns
the card to its idle state). That discipline is what lets it be the tool you run
when everything else is broken.

---

## 10.10 File and build placement

| Path                                   | Contents                            |
|----------------------------------------|-------------------------------------|
| `src/hwinfo/hwinfo.c`                  | main flow, argument parsing, UI     |
| `src/hwinfo/dosprobe.c`                | INT-based probes (memory, disk...)  |
| `src/hwinfo/cpuasm.asm`                | `cpu_class_`, `fpu_present_`         |
| `src/common/portab.h`                  | `WR()` macro, shared REGS bridge     |
| `src/common/screen.c`                  | 80x25 CP437 panel drawing (shared)  |
| `C:\CASTALIA\BIN\HWINFO.EXE`           | installed binary (on PATH)          |

`HWINFO` shares `screen.c` and `portab.h` with the other Castalia text-mode
tools (`CASTALIA.EXE`, `MEMPROF.EXE`, `SETSOUND.EXE`) so the palette and framing
stay identical across the suite. It statically links only Castalia MIT code and
the C runtime; it never links a FreeDOS/GPL object, keeping the license boundary
clean per the project rules.
```
