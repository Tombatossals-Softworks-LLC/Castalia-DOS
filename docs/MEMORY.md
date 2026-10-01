# 6. Memory Management Strategy

Memory management is where a DOS distribution is won or lost. DOS games run in
**real mode**, where the CPU can directly address only the first 1 MB, and the
usable program area — **conventional memory** — is just the first 640 KB. Every
driver, every TSR, and DOS itself competes for that 640 KB. A game that needs
590 KB free will simply refuse to start if a mouse driver and a sound TSR have
eaten the space. CASTALIA DOS solves this with **profiles**: named boot layouts
that load only what a task needs and push everything possible out of the way.

This document explains the concepts, then gives the exact `CONFIG.SYS` and
`AUTOEXEC.BAT` for each profile.

## The DOS memory map

```
  Address     Region                What lives there
  ────────────────────────────────────────────────────────────────
  0000-9FFFF  Conventional (640 KB) DOS, drivers, TSRs, and YOUR GAME
  A0000-AFFFF VGA graphics buffer    (video, not usable as RAM)
  B0000-B7FFF Mono text buffer       often free -> reclaimable as UMB
  B8000-BFFFF Colour text buffer     (video, not usable as RAM)
  C0000-EFFFF Adapter ROM / UMBs     video BIOS, option ROMs, free gaps
  F0000-FFFFF System BIOS ROM        (not usable as RAM)
  ────────────────────────────────────────────────────────────────
  100000-...  Extended memory (XMS)  needs a 286+; games access via XMS/DPMI
  100000-10FFEF  High Memory Area    the first ~64 KB above 1 MB (the "HMA")
```

## The vocabulary

| Term | What it means | Who provides it |
|---|---|---|
| **Conventional memory** | The first 640 KB, where real-mode programs load. | the machine |
| **Upper Memory (UMA)** | 640 KB–1 MB; ROMs plus free "gaps" that a memory manager can back with RAM to make **UMBs**. | EMM386/JEMM386 |
| **UMB** | Upper Memory Block: usable RAM mapped into a free upper gap; drivers/TSRs loaded here free up conventional memory. | JEMM386 + `DOS=UMB` |
| **HMA** | High Memory Area: the ~64 KB just above 1 MB, reachable in real mode via the A20 line. DOS can relocate most of itself here. | HIMEMX + `DOS=HIGH` |
| **Extended memory (XMS)** | RAM above 1 MB, offered through the XMS API. Many 1990s games use it for levels/assets. | HIMEMX |
| **Expanded memory (EMS)** | Older bank-switched memory model; a 64 KB "page frame" in upper memory windows onto a larger pool. Some older/complex games need it. | JEMM386 with a page frame |
| **HIMEM / HIMEMX** | The XMS driver; also enables the HMA. | `DEVICE=HIMEMX.EXE` |
| **EMM386 / JEMM386** | Turns extended memory into UMBs and (optionally) EMS on a 386+. | `DEVICE=JEMM386.EXE` |

## The DOS directives that move things out of the way

| Directive | Effect |
|---|---|
| `DEVICE=HIMEMX.EXE` | Load the XMS driver (must come first). |
| `DOS=HIGH` | Relocate most of DOS into the HMA — frees ~45–50 KB conventional. Needs HIMEMX. |
| `DEVICE=JEMM386.EXE NOEMS` | Provide UMBs, **no** EMS page frame (maximum conventional). |
| `DEVICE=JEMM386.EXE FRAME=E000` | Provide UMBs **and** a 64 KB EMS page frame at E000. |
| `DOS=UMB` (usually `DOS=HIGH,UMB`) | Let DOS manage and hand out UMBs. |
| `DEVICEHIGH=...` | Load a `CONFIG.SYS` driver into a UMB instead of conventional. |
| `LH` / `LOADHIGH` | Load an `AUTOEXEC.BAT` TSR into a UMB. |

## Why DOS games are hard

1. **The 640 KB ceiling is real.** No amount of extended memory helps a game
   that must load its main executable and buffers into conventional memory.
2. **Games disagree.** One title wants EMS; the next crashes if an EMS page
   frame exists. One needs XMS; another dislikes any memory manager at all.
3. **Drivers are greedy.** A mouse, a CD-ROM stack, and a sound TSR can
   together cost 60–100 KB of conventional memory if loaded low.
4. **Upper memory is scarce and machine-specific.** How much UMB a machine can
   provide depends on its chipset and adapter ROMs; a 386SX often has less than
   a 486.
5. **The page frame is a tax.** Enabling EMS reserves 64 KB of upper memory for
   the page frame, which is 64 KB less available for UMBs.

The profiles below are the answer: each is a deliberate point on the
trade-off curve between *maximum conventional memory* and *maximum
capability*.

---

## Profile: CLEAN — Maximum Compatibility

The minimal sane configuration. HIMEMX loads (so DOS can live in the HMA), and
nothing else. No EMM386, no UMBs, no page frame, no TSRs. This is the profile
to reach for when a cranky real-mode game refuses to run anywhere else.

**CONFIG.SYS lines for entry 1**
```
1234568?DEVICE=C:\DOS\HIMEMX.EXE
168?DOS=HIGH
```
The `n?` prefix is the FreeDOS kernel's own syntax: the line runs only when
one of the listed entries was chosen. The kernel has no MS-DOS-style
`[CLEAN]` blocks; see [`BOOT.md`](BOOT.md) before editing.

**AUTOEXEC.BAT behaviour:** sets the environment and prompt, sets a fallback
`BLASTER`, loads **no** mouse and **no** cache, shows the banner, and starts
the Castalia menu.

| | |
|---|---|
| **Expected conventional** | ~615 KB free on a 386SX/4 MB |
| **XMS** | available (HIMEMX) |
| **EMS / UMB** | none |
| **Advantages** | Highest compatibility; nothing to conflict with a fussy game. |
| **Disadvantages** | No expanded memory; drivers cannot load high (there are none anyway). |
| **Game examples** | Prince of Persia, Commander Keen, Monkey Island, SimCity, Lemmings |
| **Troubleshooting** | If a game still won't start, boot **SAFE** (no HIMEMX) or free more RAM by removing the fallback `BLASTER`. |

---

## Profile: XMS — XMS Gaming (default)

HIMEMX plus JEMM386 with **no** EMS page frame. This yields UMBs (so the mouse
and any TSRs load high) while reserving no page frame, which produces the
**most free conventional memory** of any capable profile. XMS is available for
the many games that use it. This is the default and the right choice for the
overwhelming majority of 1990–1995 titles.

**CONFIG.SYS lines for entry 2**
```
1234568?DEVICE=C:\DOS\HIMEMX.EXE
2?DEVICE=C:\DOS\JEMM386.EXE NOEMS I=B000-B7FF
2345?DOS=HIGH,UMB
```
`I=B000-B7FF` reclaims the unused monochrome text region as extra UMB space on
colour (VGA) systems. Remove it if a specific title misbehaves.

**AUTOEXEC.BAT behaviour:** loads `CTMOUSE` high (`LH`), sets `BLASTER`, shows
the banner, starts the menu.

| | |
|---|---|
| **Expected conventional** | ~620–631 KB free (mouse loaded high) |
| **XMS** | available |
| **EMS** | none |
| **UMB** | yes |
| **Advantages** | Best all-round profile; drivers load high; huge free conventional. |
| **Disadvantages** | No EMS for the few games that require it. |
| **Game examples** | Doom, Wolfenstein 3D, Dune II, Stunts, X-COM, Duke Nukem 3D |
| **Troubleshooting** | If a game reports "EMS required," use **EMS**. If it dislikes UMBs, use **CLEAN**. |

---

## Profile: EMS — EMS Gaming

HIMEMX plus JEMM386 **with** a 64 KB EMS page frame at E000, plus UMBs. For
titles that specifically ask for expanded memory (some Origin/Ultima setup
paths, some Sierra titles, larger Warcraft maps). The page frame costs upper
memory, so conventional is a little lower than XMS — use this profile only
when a game actually needs EMS.

**CONFIG.SYS lines for entry 3**
```
1234568?DEVICE=C:\DOS\HIMEMX.EXE
345?DEVICE=C:\DOS\JEMM386.EXE FRAME=E000 I=B000-B7FF
2345?DOS=HIGH,UMB
```

**AUTOEXEC.BAT behaviour:** identical to XMS (mouse high, `BLASTER`, menu) —
the difference is entirely in `CONFIG.SYS` (the page frame).

| | |
|---|---|
| **Expected conventional** | ~600 KB free |
| **XMS** | available |
| **EMS** | available (64 KB frame at E000) |
| **UMB** | yes |
| **Advantages** | Satisfies EMS-aware games; still loads drivers high. |
| **Disadvantages** | Page frame lowers conventional and reduces UMB space. |
| **Game examples** | Ultima VI/VII setup, some Sierra AGI/SCI titles, Warcraft |
| **Troubleshooting** | If EMS isn't detected, confirm `FRAME=` isn't colliding with an option ROM; try `FRAME=D000`. |

---

## Profile: CDROM — CD-ROM Gaming

The EMS base plus the CD-ROM stack: `UIDE.SYS` (a universal IDE/ATAPI driver,
loaded in `CONFIG.SYS`) binds the drive to the name `CASTLCD1`, and
`SHSUCDX` (an MSCDEX replacement, loaded high in `AUTOEXEC.BAT`) assigns it a
drive letter (D: by default). The sound environment is set as usual.

**CONFIG.SYS lines for entry 4**
```
1234568?DEVICE=C:\DOS\HIMEMX.EXE
345?DEVICE=C:\DOS\JEMM386.EXE FRAME=E000 I=B000-B7FF
2345?DOS=HIGH,UMB
4?DEVICE=C:\CASTALIA\DRV\UIDE.SYS /D:CASTLCD1 /N1 /N3
```
`/N1` limits UIDE to CD/DVD drives and `/N3` runs it without an XMS cache,
which it refuses to do from upper memory, hence `DEVICE` rather than
`DEVICEHIGH`. The reasons are spelled out in `config/CONFIG.SYS`.

**AUTOEXEC.BAT behaviour (CDROM branch):**
```
IF EXIST C:\CASTALIA\DRV\CTMOUSE.EXE LH C:\CASTALIA\DRV\CTMOUSE.EXE
IF EXIST C:\DOS\SHSUCDX.COM LH C:\DOS\SHSUCDX.COM /D:CASTLCD1 /L:D
```

| | |
|---|---|
| **Expected conventional** | ~585 KB free (CD stack loaded high) |
| **XMS / EMS / UMB** | all available |
| **CD-ROM** | drive D: (via UIDE + SHSUCDX) |
| **Advantages** | CD games work; drive letter is predictable; sound is set. |
| **Disadvantages** | The CD driver + MSCDEX cost memory even loaded high. |
| **Game examples** | The 7th Guest, CD "talkie" adventures, CD strategy titles |
| **Troubleshooting** | No drive letter? Check that `UIDE` found the drive (watch the boot messages) and that `LASTDRIVE` is high enough. Wrong letter? Change `/L:D`. |

---

## Profile: WIN3X — Windows 3.x Mode

Windows 3.1 and Windows for Workgroups 3.11 prefer a system with an EMM
present, a mouse, and a disk cache. This profile provides HIMEMX + JEMM386
(with EMS) + UMBs, loads `CTMOUSE`, and starts `SMARTDRV` if you have put
one in `C:\DOS`. No cache ships with CASTALIA DOS: Microsoft's `SMARTDRV` is
not redistributable.

**CONFIG.SYS lines for entry 5**
```
1234568?DEVICE=C:\DOS\HIMEMX.EXE
345?DEVICE=C:\DOS\JEMM386.EXE FRAME=E000 I=B000-B7FF
2345?DOS=HIGH,UMB
```

**AUTOEXEC.BAT behaviour (WIN3X branch):**
```
IF EXIST C:\CASTALIA\DRV\CTMOUSE.EXE LH C:\CASTALIA\DRV\CTMOUSE.EXE
IF EXIST C:\DOS\SMARTDRV.EXE LH C:\DOS\SMARTDRV.EXE C 1024 0
```
(`C` caches drive C: for reads only, so nothing is held back from the disk;
`1024 0` is a 1 MB cache that Windows may not shrink. The second number is
the Windows cache size, not a write-behind switch.)

| | |
|---|---|
| **Expected conventional** | ~590 KB free (Windows manages its own memory once started) |
| **XMS / EMS / UMB** | all available |
| **Advantages** | Comfortable base for Windows 3.1 (standard mode) and light Windows apps. |
| **Disadvantages** | **Enhanced mode needs a 386DX or better**; a 386SX runs Windows only in standard mode and slowly. |
| **Notes** | Type `WIN` to start Windows, or use the Castalia menu. |
| **Troubleshooting** | If Windows enhanced mode fails on a 386SX, that is expected — use standard mode (`WIN /s`) or stay in DOS. |

---

## Profile: SAFE — Safe Mode (troubleshooting)

The bare kernel and shell. No HIMEMX, no EMM386, no drivers, no TSRs; DOS
stays in low memory. Use it to recover a machine when a driver or profile
misbehaves, or to give a truly hostile real-mode game every last byte.

**CONFIG.SYS lines for entry 7**
```
7?DOS=LOW
```

**AUTOEXEC.BAT behaviour:** trims `PATH`, prints a rescue banner listing
`SAFEBOOT`, `CFGEDIT`, `HWINFO`, and `MEM /C`, then stops at the prompt. It
does **not** start the menu.

| | |
|---|---|
| **Expected conventional** | ~635 KB free (nothing loaded) |
| **XMS / EMS / UMB / CD / mouse / sound** | none |
| **Advantages** | Boots when everything else is broken; maximum conventional. |
| **Disadvantages** | No amenities at all. |
| **When to use** | A bad edit to `CONFIG.SYS`, a driver that hangs the boot, or diagnosing a memory conflict. |

---

## DIAG and PROMPT

- **DIAG** uses the CLEAN base (HIMEMX + `DOS=HIGH`) and simply runs
  `HWINFO.EXE` from `AUTOEXEC.BAT`, then leaves you at the prompt. It is for
  inspecting the machine without touching the game profiles.
- **PROMPT** also uses the CLEAN base but skips the menu entirely and drops to
  a bare command prompt — for power users and scripting.

Both are defined in [`config/CONFIG.SYS`](../config/CONFIG.SYS).

---

## Free-memory expectations (summary)

Numbers are approximate for a 386SX/16 with 4 MB and a typical chipset; a 486
with more reclaimable upper memory usually does a little better.

| Profile | HIMEMX | JEMM386 | Page frame | UMB | Free conventional |
|---|:---:|:---:|:---:|:---:|---:|
| SAFE   | – | – | – | – | ~635 KB |
| CLEAN  | ✓ | – | – | – | ~615 KB |
| XMS    | ✓ | ✓ NOEMS | – | ✓ | ~628 KB |
| EMS    | ✓ | ✓ | E000 | ✓ | ~600 KB |
| CDROM  | ✓ | ✓ | E000 | ✓ | ~585 KB |
| WIN3X  | ✓ | ✓ | E000 | ✓ | ~590 KB |

> Note that XMS can beat CLEAN for *free conventional* despite loading more,
> because loading the mouse (and DOS structures) **high** into UMBs more than
> pays back the small cost of JEMM386. That is the whole point of UMBs.

## How to check on the real machine

Boot the profile, then run `MEM /C /P` (page the classified report). The
"Largest executable program size" line is what a game sees. Compare it against
the game's stated requirement. `HWINFO.EXE` shows the same figures in the
Castalia UI along with which profile you booted.

## Verifying against these targets

The test matrix in [`TESTING.md`](TESTING.md) includes `MEM-*` cases that
measure free conventional memory per profile against the numbers above, and
checks that UMB load-high actually happened and that the EMS page frame is
present (EMS/CDROM/WIN3X) or absent (XMS/CLEAN) as designed.
