# The CASTALIA Kernel

CASTALIA DOS boots the **Castalia kernel** — the FreeDOS 1.3 kernel (build
2043) rebuilt from source with a small, deliberate set of Castalia
modifications. This document is the engineering reference for those changes and
the **register-level contract** that the Castalia tools rely on.

The guiding rule is the same one that governs the whole product: *compatibility
beats elegance*. The Castalia kernel is a **superset** of the FreeDOS kernel. It
adds identity and introspection; it removes nothing and changes no documented
DOS behaviour. A stock DOS program cannot tell the difference — until it asks.

> Why not just change the banner? Because a renamed binary is still someone
> else's binary. The Castalia kernel earns its name: it reports its own OEM
> identity, it records which boot profile is running, and it answers a Castalia
> identity call that our own tools query live. It stays 100% FreeDOS-compatible
> and keeps full FreeDOS attribution (it is GPLv2+; see
> [`LICENSE-STRATEGY.md`](LICENSE-STRATEGY.md) §2.3.2a) — the point is not to
> *hide* the lineage but to make the derivative genuine.

All changes are applied from source at build time by
[`scripts/patch-kernel-src.py`](../scripts/patch-kernel-src.py), driven by
[`scripts/build-kernel.sh`](../scripts/build-kernel.sh). They are anchored on
unique upstream text, idempotent, and fail loudly if upstream moves.

---

## 1. What changed, at a glance

| Area | Stock FreeDOS 2043 | Castalia kernel | File patched |
|---|---|---|---|
| Boot sign-on | `FreeDOS kernel … WATCOMC …` | `CASTALIA DOS 386SX Edition` | `kernel/main.c` |
| OEM identity (`INT 21h`/`30h` `BH`) | `0xFD` | `0xCA` | `hdr/version.h`, `kernel/kernel.asm` |
| Boot-profile record | *(none)* | `castalia_boot_profile` byte | `kernel/globals.h` |
| Boot-tick stamp | *(none)* | `castalia_boot_tick` (uptime) | `kernel/globals.h`, `kernel/main.c` |
| Init-code declarations | *(none)* | both symbols made visible to init | `kernel/init-mod.h` |
| `CONFIG.SYS` directive | *(none)* | `CASTALIA=n` | `kernel/config.c` |
| Identity syscall | *(none)* | `INT 2Fh` `AH=0CAh` | `kernel/int2f.asm` |

Everything else is byte-for-byte FreeDOS 1.3 build 2043. The kernel is shipped
**uncompressed** (no UPX), which is what lets the sign-on be plain ASCII in the
image.

The shared constants (kept in sync between the kernel patch and the userland
tools) are:

| Name | Value | Meaning |
|---|---|---|
| OEM id | `0xCA` | "Castalia", in `INT 21h`/`30h` `BH` and `INT 2Fh`/`CA00h` `DL` |
| Signature | `0xCA5A` | confirmation word returned in `BX` |
| Build | `1` | Castalia kernel build number, returned in `CX` |
| Edition | `0x01` | `01` = 386SX Edition, returned in `DH` |

---

## 2. The identity multiplex — `INT 2Fh`, `AH = 0CAh`

`0xCA` lives in the *application* range of the `INT 2Fh` multiplex
(`0xC0–0xFF`). The FreeDOS kernel installs the **bottom** handler in the
`INT 2Fh` chain and `iret`s for any function it does not own, so intercepting
`0xCA` at the top of that handler affects nothing else — any real TSR that also
used `0xCA` would sit *above* the kernel in the chain and answer first.

The handler preserves `SI`, `DI`, `BP`, `ES` and `DS`; it returns values only in
`AX`, `BX`, `CX`, `DX`. It never sets carry and never chains.

### `AX = CA00h` — identity

```
In:   AX = CA00h
Out:  AL = FFh          installed marker (00h → not the Castalia kernel)
      BX = CA5Ah        signature (confirm this before trusting the call)
      CX = build number  (Castalia kernel build, e.g. 1)
      DH = edition       (01h = 386SX Edition)
      DL = OEM id        (CAh)
```

### `AX = CA01h` — active boot profile

```
In:   AX = CA01h
Out:  AL = FFh          installed marker
      BX = CA5Ah        signature
      CL = profile       (1..8, or 0 when no CASTALIA= directive ran)
      CH = 0
```

Profile codes match the boot menu order: `1` CLEAN, `2` XMS, `3` EMS, `4` CDROM,
`5` WIN3X, `6` DIAG, `7` SAFE, `8` PROMPT.

### `AX = CA02h` — boot tick (uptime)

```
In:   AX = CA02h
Out:  AL = FFh          installed marker
      BX = CA5Ah        signature
      CX:DX = the BIOS tick (0040:006C) stamped when the kernel signed on
              (CX = high word, DX = low word)
```

`signon()` runs exactly once, unconditionally, at boot, so it is where the
kernel stamps `castalia_boot_tick`. A caller computes uptime by reading the
live BIOS tick and subtracting: at 18.2 ticks/second,
`seconds = (now - boot) * 10 / 182`. The counter is reset by the BIOS at
midnight, so a session spanning midnight reads low — treat a `now < boot`
result as "wrapped" rather than as an error.

Any other `AL` under `AH=0CAh` returns unhandled (`AL` left unchanged), leaving
room to grow the API without breaking callers.

### Detecting it correctly

Always require **both** `AL == FFh` **and** `BX == CA5Ah`. On a stock kernel the
call falls through to `iret` with the registers unchanged (`AL` stays `00h`), so
the `AL` check alone is enough; the `BX` signature guards against an unrelated
`0xCA` TSR that happens to answer.

### Reference caller (16-bit Watcom C)

```c
#include <dos.h>

/* Returns the Castalia kernel build number (>=1), or 0 on a stock kernel. */
static int castalia_kernel(int *edition, int *oem)
{
    union REGS r;
    r.x.ax = 0xCA00;
    int86(0x2F, &r, &r);
    if (r.h.al != 0xFF || r.x.bx != 0xCA5A)
        return 0;                       /* not the Castalia kernel */
    if (edition) *edition = r.h.dh;
    if (oem)     *oem     = r.h.dl;
    return r.x.cx;
}
```

This is exactly the probe used by [`HWINFO.EXE`](../src/hwinfo/HWINFO.C) (which
shows a **Kernel** row) and the [`CASTALIA.EXE`](../src/castalia/CASTALIA.C)
**About** screen (which shows a live `Castalia kernel: build N` line). That is
the loop the whole exercise closes: the kernel is genuinely different, and the
tools genuinely read it.

---

## 3. OEM identity — `INT 21h`, `AH = 30h`

`INT 21h`/`AH=30h` (Get DOS Version) returns the OEM vendor byte in `BH`. Stock
FreeDOS returns `0xFD`; the Castalia kernel returns **`0xCA`**. The reported
DOS version number (`AL.AH`) is unchanged — it stays FreeDOS's MS-DOS-compatible
`7.10` (FAT32 build), because games detect DOS by version and that must not move.

Changing the OEM byte is safe: it is a documented vendor field, and software that
special-cases specific vendors keys on `0x00` (IBM/MS) or `0xEE` (DR-DOS), never
on an arbitrary value — a stock FreeDOS already ships the non-standard `0xFD`
and runs the DOS catalogue fine. `0xCA` is treated identically (as "generic").

---

## 4. The `CASTALIA=` CONFIG.SYS directive

```
CASTALIA=n        ; n = active boot profile (1..8)
```

Parsed by `CfgCastalia()` in `kernel/config.c` (registered in the directive
table at pass 1). It reads one integer and stores it in the resident
`castalia_boot_profile` byte, which the `INT 2Fh`/`CA01h` call returns. An
empty or non-numeric value leaves the byte unset (`0`); the directive is never
fatal, and a non-Castalia kernel simply ignores an unknown directive.

The shipped configs set it per profile — see
[`config/CONFIG.SYS`](../config/CONFIG.SYS) (the eight-profile boot menu) and
[`config/floppy/CONFIG.SYS`](../config/floppy/CONFIG.SYS) (the rescue diskette,
which declares `CASTALIA=7`). This is what makes the active profile
*kernel-authoritative* instead of merely echoed by a batch file.

---

## 5. Building the Castalia kernel

```sh
scripts/build-kernel.sh          # -> build/KERNEL.SYS (uncompressed, Castalia)
```

Requirements: Open Watcom V2 (`wmake`/`wlink`), `gcc`, `nasm`, `make`, plus
`curl`/`unzip`/`python3`. The CI **DOS build** job installs these and runs the
script; the **e2e** job then boots the resulting kernel in DOSBox. The script
verifies, at the source level, that every Castalia modification took before it
spends a build, and, at the binary level, that the CASTALIA sign-on is present
and the FreeDOS "Kernel compatibility" sign-on is gone.

### Gotcha: the init code cannot see `globals.h`

`kernel/main.c` and `kernel/config.c` are **init** modules: they include
`init-mod.h`, *not* `globals.h`. FreeDOS therefore re-declares every resident
symbol the init code touches inside `init-mod.h`
(`extern BYTE DOSFAR ASM ReturnAnyDosVersionExpected;` and friends). A Castalia
symbol added only to `globals.h` looks fine everywhere — and then the real
Open Watcom build stops with:

```
config.c(2198): Error! E1011: Symbol 'castalia_boot_profile' has not been declared
```

which is exactly how the first cut of this patch set broke CI. Both
`castalia_boot_profile` and `castalia_boot_tick` are consequently declared in
**both** headers, and `scripts/build-kernel.sh` enforces the rule with a
source-level invariant (every `castalia_*` identifier used in `main.c` or
`config.c` must appear in `init-mod.h`) that runs *before* the compiler.

### Corresponding source (GPLv2)

The Castalia kernel is a GPLv2 §2 derivative of FreeDOS. Its *complete
corresponding source* is the pinned upstream `ke2043s.zip` **plus**
`scripts/patch-kernel-src.py` and `scripts/build-kernel.sh`, which apply and
record every change. A release additionally carries the FreeDOS source in
`third_party/`. See [`LICENSE-STRATEGY.md`](LICENSE-STRATEGY.md) §2.3.2a.

### Bumping the build number

The Castalia build number is defined once as `CAST_BUILD` in
`scripts/patch-kernel-src.py` (emitted into the `INT 2Fh`/`CA00h` handler) and
mirrored in the userland reference above. Increment it when a kernel change ships
so `HWINFO`/`About` report the new build.

---

*The Castalia kernel: FreeDOS underneath, Castalia by identity — and honest
about both.*
