# 1. Executive Vision

## What CASTALIA DOS is

CASTALIA DOS is a DOS-compatible operating environment built to be a
practical, beautiful replacement for MS-DOS 6.22 on real 386SX, 386DX, 486,
and early Pentium machines. Its purpose is narrow and honest: **run DOS games
and light retro productivity software well on real hardware**, with a level of
polish, documentation, and configuration convenience that the original
MS-DOS never offered.

It is a *distribution* first and a *platform* second. Version 1.0 takes the
FreeDOS kernel and FreeCOM shell — mature, legal, open-source components that
already provide MS-DOS 5.0/6.22-style behaviour — and surrounds them with
original Castalia work:

- A boot menu with eight purpose-built **memory profiles**.
- A text-mode **game launcher** (`LAUNCH.EXE`) driven by a game database.
- A **main menu** (`CASTALIA.EXE`) that ties the tools together.
- **Sound**, **memory**, and **CD-ROM** configuration helpers.
- **Hardware diagnostics** and **rescue** tools.
- A clean **installer**, **documentation set**, and a coherent **visual
  identity**.

## What CASTALIA DOS is not

- It is **not** a clone of MS-DOS and contains no Microsoft code, binaries,
  message text, manuals, or branding. Where it needs a DOS kernel, it uses
  FreeDOS.
- It is **not** a protected-mode operating system, a multitasker, or a
  Windows 95 replacement. Those goals fight the one goal that matters here:
  running real-mode DOS software the way it expects.
- It is **not** a modern OS wearing a retro skin. The core deliberately
  embraces real-mode DOS and its quirks.

## The guiding philosophy: compatibility beats elegance

DOS games were written against the actual behaviour of DOS — its interrupts,
its memory map, its timing, its device conventions. A "better" core that
changes those behaviours breaks the software we are trying to run. So the
core stays faithfully DOS-like. All of Castalia's ambition goes into the
*experience around* DOS, never into modernising the parts games depend on:

| We deliberately keep faithful to DOS | We make dramatically better |
|---|---|
| Real-mode execution model | Boot menu & memory profiles |
| INT 21h / INT 13h / INT 10h behaviour | Game launcher & main menu |
| Conventional / UMB / XMS / EMS memory map | Sound configuration |
| FAT12 / FAT16 on-disk format | Documentation & help |
| TSR and driver conventions | Installer & first-run setup |
| Program load / exit semantics | Diagnostics & rescue tools |

## Why a FreeDOS-based distribution — not a clean-room MS-DOS clone

Writing a new DOS kernel from scratch is a multi-year effort with enormous
compatibility risk, and it delivers *nothing the user can see* that FreeDOS
does not already deliver. The value of CASTALIA DOS lives almost entirely in
the layers above the kernel. Building on FreeDOS is the correct engineering
decision for five concrete reasons:

1. **It is legal and open.** FreeDOS is GPL-licensed, freely redistributable,
   and explicitly intended to be built into custom distributions. We can ship
   it, brand our own layers, and stay clean of Microsoft's copyrights and
   trademarks.
2. **It already works.** FreeDOS boots real hardware today, runs the vast
   majority of DOS games, supports FAT12/FAT16, and implements MS-DOS-style
   APIs and the CONFIG.SYS menu system we rely on.
3. **The ecosystem exists.** Memory managers (HIMEMX, JEMM386), a BSD mouse
   driver (CTMOUSE), CD-ROM support (UIDE + SHSUCDX), keyboard layouts, and
   utilities are all open and battle-tested. We assemble; we do not reinvent.
4. **Effort goes where it is visible.** Every hour we would have spent
   reimplementing `INT 21h` is instead spent on the launcher, the profiles,
   the installer, and the docs — the things a user actually touches.
5. **It leaves the door open.** Nothing about starting on FreeDOS prevents us
   from replacing components later. We can swap the shell, add original
   drivers, or — much later — explore an original kernel, one piece at a time,
   without ever breaking the shipping product.

A clean-room MS-DOS clone would be a research project. CASTALIA DOS is a
product. We ship the product.

## Three layers of the idea (do not confuse them)

It helps to name three distinct things that share the "Castalia" name, so the
roadmap stays honest about scope.

### 1. CASTALIA DOS — a practical FreeDOS-based operating environment (this project, 1.0)

A polished, bootable distribution: FreeDOS core + Castalia profiles, tools,
installer, docs, and branding. This is what 1.0 "Tombatossals" is. It is
achievable, legal, and genuinely useful, and it is the entire focus of the
current work. Everything in this bible that carries a version number ≤ 1.1
belongs here.

### 2. CASTALIA DOS as a deeper, custom DOS platform (future fork)

Over time, individual FreeDOS components can be replaced or extended with
original Castalia code — a Castalia shell, Castalia drivers, Castalia-specific
kernel patches, perhaps eventually an original kernel. This is a *gradual*
evolution of the same product, undertaken only when a replacement is clearly
better and does not cost compatibility. It is explicitly **not** a 1.0 goal;
it is the long tail (see Phase 10 in [`ROADMAP.md`](ROADMAP.md)).

### 3. CASTALIA OS / CASTALIA DESK — a separate future graphical shell

A future, optional, graphical program-manager-style shell (working name
**CASTALIA DESK**) that runs *on top of* CASTALIA DOS, launched from the
command line, never resident, and never required for game compatibility. It
is a different product concern from the DOS core and is kept out of 1.0 on
purpose. See [`FUTURE-SHELL.md`](FUTURE-SHELL.md). Keeping it separate is what
protects the 386SX performance budget and the compatibility promise.

## What 1.0 focuses on

CASTALIA DOS 1.0 succeeds if, on a real 386SX with a floppy and a
CompactFlash card, a user can:

1. **Boot** reliably from floppy, hard disk, or CF.
2. **Install** cleanly to a FAT16 disk, with the existing CONFIG.SYS and
   AUTOEXEC.BAT safely backed up first.
3. **Choose a memory profile** from a clear boot menu and get the most free
   conventional memory appropriate to the task.
4. **Launch a game** from a keyboard-driven launcher that warns about profile
   and CD mismatches and gets out of memory before the game runs.
5. **Configure sound** with a helper that writes a correct `SET BLASTER`
   line instead of guessing.
6. **Diagnose the machine** when something is wrong, and **recover** it from a
   Safe Mode or a rescue floppy.
7. **Read good documentation** that fits an 80×25 screen and speaks plainly.

Everything else — the file manager, the graphical shell, an original kernel —
waits. The first version must be achievable, legal, bootable, and genuinely
useful. That is the whole ambition of 1.0, and it is enough.

## Assumptions recorded

Because this bible chooses concrete paths, the load-bearing assumptions are
written down here so they can be revisited:

- **Primary target is a 386SX/16 with ~4 MB RAM, VGA, IDE/CF, and a floppy.**
  Tools are budgeted against this machine, not a 486.
- **FreeDOS is the 1.0 kernel and shell.** Original components replace it only
  when clearly better.
- **VGA 80×25 text mode is the only required display mode.** EGA/CGA fallback
  and any graphics are later, optional concerns.
- **The mouse is always optional.** No Castalia tool requires it.
- **Memory managers are HIMEMX + JEMM386**, with FreeDOS EMM386 as a fallback.
- **The reported DOS version is FreeDOS's**; per-program version needs are met
  with `SETVER`, not a global spoof. We advertise "6.22-compatible behaviour,"
  not "we are MS-DOS 6.22."

These assumptions are the ground the rest of the bible builds on.
