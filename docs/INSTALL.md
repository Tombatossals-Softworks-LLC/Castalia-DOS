# 11. Installer Design & Installation Guide

`SETUP.EXE` is the CASTALIA DOS installer. It is a text-mode program that runs
from the boot floppy (or from an existing DOS system) and installs CASTALIA DOS
to a hard disk or CompactFlash card. Its two non-negotiable jobs are: **never
lose the user's data or existing config**, and **leave a bootable, working
system**.

> **Implementation status:** an initial prototype exists at
> [`../src/setup/SETUP.C`](../src/setup/SETUP.C) (`wmake setup`). It runs the
> full wizard and does the safe work described below — backup, tree creation,
> `XCOPY` of the DOS/Castalia layers, writing the boot files and sound profile,
> and an optional `SYS` to make the disk bootable. The prototype deliberately
> does **not** partition or format a disk; it targets an already-formatted
> FAT16 volume, leaving `FDISK`/`FORMAT` to the user. The partition/format
> screens, disk detection, and multi-floppy source flow below describe the
> intended full design.

## What the installer supports

- Boot-floppy installation and installation from an existing DOS/FreeDOS system
- Detection of an existing DOS installation on the target
- Backup of any existing `CONFIG.SYS` and `AUTOEXEC.BAT` before touching them
- FAT16 target partitions (create/format or use an existing one)
- FreeDOS component installation (kernel, shell, memory managers, utilities)
- Castalia tools installation
- Driver selection (mouse, CD-ROM)
- Keyboard-layout selection
- Mouse selection (serial / PS-2 / none)
- Sound-profile selection (writes an initial `SOUND.BAT`)
- CD-ROM support (installs `UIDE` + `SHSUCDX`, wires the CDROM profile)
- Game-folder creation (`C:\GAMES`, `C:\CASTALIA\GAMES`)
- Emergency boot-disk creation

## Installation flow

```
SETUP.EXE
  1  Welcome + license summary            (accept to continue)
  2  Choose target disk                   (list disks; show sizes)
  3  Partition & format?                  (existing FAT16 | create+format)
        - if create: FDISK-style partition, mark active, FORMAT /S-style
  4  Detect existing DOS                  (warn; back up its config)
  5  Choose options
        - keyboard layout (US default)
        - mouse (serial COM1/COM2 | PS-2 | none)
        - sound profile (None..SB16)
        - CD-ROM support (yes/no)
  6  Confirm plan                         (show exactly what will happen)
  7  Copy files                           (DOS, CASTALIA, config)
  8  Write boot sector + kernel           (SYS the target)
  9  Write CONFIG.SYS / AUTOEXEC.BAT      (from templates + chosen options)
 10  Create C:\GAMES, C:\CASTALIA\BACKUP
 11  Offer to build an emergency boot disk
 12  Done + reboot prompt
```

Each step is reversible up to step 7. Steps 7–9 are the only destructive ones,
and they are preceded by the explicit confirmation in step 6.

## UI mockups (80×25)

**Welcome**
```
 CASTALIA DOS Setup                                    386SX Edition 1.0
 ┌──────────────────────────────────────────────────────────────────────┐
 │                                                                        │
 │   Welcome to CASTALIA DOS.                                             │
 │                                                                        │
 │   This installer will set up a game-focused, FreeDOS-based DOS         │
 │   environment on your hard disk or CompactFlash card.                  │
 │                                                                        │
 │   Setup will:                                                          │
 │     - back up any existing CONFIG.SYS and AUTOEXEC.BAT                 │
 │     - install the DOS core, Castalia tools, and boot profiles         │
 │     - make the disk bootable                                          │
 │                                                                        │
 │   Nothing is changed until you confirm the plan.                       │
 │                                                                        │
 └──────────────────────────────────────────────────────────────────────┘
   Enter Continue      F1 Help      Esc Cancel
```

**Choose target**
```
 CASTALIA DOS Setup   -   Choose install disk
 ┌──────────────────────────────────────────────────────────────────────┐
 │   Disk    Size     Type        Bootable   Existing system            │
 │   ─────────────────────────────────────────────────────────────────  │
 │ ► C:      504 MB   FAT16 IDE   yes        MS-DOS 6.22 detected        │
 │   D:      120 MB   FAT16 CF    no         (empty)                     │
 │                                                                        │
 │   WARNING: installing over an existing system will back up its        │
 │   CONFIG.SYS and AUTOEXEC.BAT to C:\CASTALIA\BACKUP, but game and      │
 │   data files are left untouched.                                      │
 └──────────────────────────────────────────────────────────────────────┘
   ↑↓ Select   Enter Continue   Esc Back
```

**Confirm plan**
```
 CASTALIA DOS Setup   -   Confirm
 ┌──────────────────────────────────────────────────────────────────────┐
 │   Target ........ C:  (FAT16, existing partition, NOT reformatted)     │
 │   Back up ....... C:\CONFIG.SYS, C:\AUTOEXEC.BAT -> C:\CASTALIA\BACKUP │
 │   Install ....... C:\DOS, C:\CASTALIA  (~3.4 MB)                       │
 │   Boot .......... write FreeDOS boot sector + KERNEL.SYS               │
 │   Keyboard ...... US        Mouse ...... serial COM1                   │
 │   Sound ......... SB Pro    CD-ROM ..... yes (drive D:)                │
 │                                                                        │
 │   Proceed?  This will write to disk C:.                                │
 └──────────────────────────────────────────────────────────────────────┘
   Enter Install      Esc Cancel
```

## Required files (what the installer lays down)

| Group | Files |
|---|---|
| Boot | FreeDOS boot sector, `KERNEL.SYS`, `COMMAND.COM` |
| DOS core (`C:\DOS`) | `HIMEMX.EXE`, `JEMM386.EXE`, `SHSUCDX.COM`, `KEYB.EXE`, `KEYBOARD.SYS`, `SETVER.EXE`, `MEM.EXE`, `FDISK.EXE`, `FORMAT.EXE`, `SYS.COM`, `CHKDSK.EXE`, `XCOPY.EXE`, `EDIT`, `SMARTDRV.EXE` |
| Castalia drivers (`C:\CASTALIA\DRV`) | `CTMOUSE.EXE`, `UIDE.SYS` |
| Castalia tools (`C:\CASTALIA\BIN`) | `CASTALIA.EXE`, `LAUNCH.EXE`, `MEMPROF.EXE`, `SETSOUND.EXE`, `HWINFO.EXE`, `CFGEDIT.EXE`, `SAFEBOOT.EXE`, `GAMECFG.EXE`, `GAMES.BAT` |
| Config (`C:\CASTALIA\CFG`) | `CASTALIA.INI`, `PROFILES.INI`, `GAMES.INI`, `SOUND.BAT` |
| Help (`C:\CASTALIA\HELP`) | help text pages |
| Root | `CONFIG.SYS`, `AUTOEXEC.BAT` |

## Target directory structure

```
C:\CASTALIA          root of the Castalia layer
C:\CASTALIA\BIN      Castalia tools (on PATH)
C:\CASTALIA\DRV      Castalia-bundled drivers (mouse, CD-ROM)
C:\CASTALIA\CFG      INI files + generated SOUND.BAT
C:\CASTALIA\HELP     plain-text help pages
C:\CASTALIA\GAMES    optional bundled/sample games
C:\CASTALIA\TOOLS    extra utilities
C:\CASTALIA\BACKUP   backups of your CONFIG.SYS / AUTOEXEC.BAT
C:\DOS               FreeDOS base + memory managers
C:\GAMES             your games
```

### Which files go where — and why

- **Root (`C:\`)** holds only what the boot process needs first: `KERNEL.SYS`,
  `COMMAND.COM`, `CONFIG.SYS`, `AUTOEXEC.BAT`. Keeping the root clean makes the
  system legible and the boot predictable.
- **`C:\DOS`** is the FreeDOS layer (GPL components) plus the memory managers.
  Isolating it keeps the GPL-licensed pieces together and makes them easy to
  update or replace.
- **`C:\CASTALIA`** is the entire original Castalia layer (MIT), split so that
  binaries (`BIN`), drivers (`DRV`), settings (`CFG`), docs (`HELP`), and
  backups (`BACKUP`) are each in one obvious place. `BIN` is on the `PATH`.
- **`C:\GAMES`** is the user's; the installer creates it but never writes game
  data there. This separation is what lets a reinstall or upgrade leave games
  untouched.

## Floppy-disk-set strategy

For the earliest releases (0.1/0.2), CASTALIA DOS ships as a **single bootable
1.44 MB floppy** plus a **hard-drive install ZIP** (see
[`DISTRIBUTION.md`](DISTRIBUTION.md)). When the payload outgrows one disk, the
multi-floppy set is:

| Disk | Label | Contents |
|---|---|---|
| 1 | BOOT/SETUP | boot sector, kernel, shell, `SETUP.EXE`, memory managers, rescue tools |
| 2 | DOS | remaining `C:\DOS` utilities |
| 3 | CASTALIA | Castalia tools, config, help |
| (4) | EXTRAS | optional drivers, sample games |

`SETUP.EXE` prompts for each disk in turn and verifies a label/marker file
before copying, so disks inserted out of order are caught.

## Safety checks

- **Free-space check** before copying (refuse if the target can't hold the
  payload).
- **Existing-config backup** is mandatory and happens before any root file is
  written. If it fails, Setup stops with the root files untouched.

What ends up in `C:\CASTALIA\BACKUP` and `C:\CASTALIA\CFG`:

| File | Written by | Holds |
|---|---|---|
| `BACKUP\CONFIG.ORG`, `AUTOEXEC.ORG` | first Setup run only | the pre-Castalia files; nothing ever rewrites them |
| `BACKUP\CONFIG.SYS`, `AUTOEXEC.BAT` | Setup, the menu's Backup, CFGEDIT | the working backup SAFEBOOT restores |
| `BACKUP\CONFIG.OLD`, `AUTOEXEC.OLD` | a repeat Setup run | the config that run replaced |
| `BACKUP\CONFIG.SAF`, `AUTOEXEC.SAF` | SAFEBOOT "write minimal config" | the config it replaced |
| `CFG\*.USR` | Setup, left behind only if it cannot put them back | your `CASTALIA.INI`, `PROFILES.INI`, `GAMES.INI`, set aside while the media's files are copied; rename them back to `.INI` |

Re-running Setup keeps your own `CASTALIA.INI`, `PROFILES.INI` and
`GAMES.INI`; it does not reset them to the shipped versions.
- **Write confirmation** (step 6) lists every destructive action explicitly.
- **Boot-sector write** only after files are copied, so a failure mid-copy
  leaves the old system still bootable.
- **CF geometry check**: warn if the BIOS geometry looks inconsistent with the
  media (a common CF-on-old-BIOS pitfall).
- **Active-partition check**: ensure exactly one active primary partition.

## Rollback plan

Because the destructive steps are ordered *copy → SYS → write config*, and the
old `CONFIG.SYS`/`AUTOEXEC.BAT` are backed up first, recovery is always
possible:

1. If Setup fails **before** the SYS step, nothing bootable changed — the old
   system still boots.
2. If Setup fails **after** SYS but before writing config, the disk boots
   CASTALIA DOS to a Safe-Mode prompt; re-run Setup or restore from backup.
3. If the new system misbehaves, boot the **emergency boot disk** and run
   `SAFEBOOT.EXE`, which restores `CONFIG.SYS`/`AUTOEXEC.BAT` from
   `C:\CASTALIA\BACKUP` (and can re-`SYS` the disk).

## Error messages (Castalia tone)

Calm, specific, never blaming the user. Examples:

| Situation | Message |
|---|---|
| Not enough space | `The target disk has 2.1 MB free; Setup needs 3.4 MB. Free some space or choose another disk.` |
| No target found | `No fixed disk was found. Connect a hard disk or CompactFlash card, or install to a different drive.` |
| Backup failed | `Setup could not back up your existing CONFIG.SYS. Nothing has been changed. Check the disk and try again.` |
| Boot-sector write failed | `Setup copied the files but could not make the disk bootable. Boot the rescue floppy and run SAFEBOOT to finish.` |
| CF geometry mismatch | `This card reports an unusual geometry. Setup can continue, but if the machine will not boot, re-create the card with matching geometry.` |

## Post-install first run

On the first boot after install, `AUTOEXEC.BAT` starts `CASTALIA.EXE`. A first
run can (in a later version) offer a short guided setup: confirm sound, add the
first games to `GAMES.INI`, and point at the help system. For 1.0 the menu
itself is the first-run experience, with **Sound Setup** and **Launch Games**
being the two things a new user reaches for.
