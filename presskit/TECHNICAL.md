# TECHNICAL DETAILS — CASTALIA DOS

For technically-minded coverage: what it is under the hood, what we used to make
it, and how it's built and verified.

## Architecture

CASTALIA DOS is an **aggregation**, not a fork: original Castalia programs run
*on top of* an open-source DOS, talking to it only through the documented INT 21h
system-call interface. Nothing Castalia is linked into the kernel or shell.

```
BIOS ─► boot sector ─► KERNEL.SYS ─► CONFIG.SYS (8-profile menu)
                                    ─► COMMAND.COM ─► AUTOEXEC.BAT
                                    ─► BANNER (keep splash) ─► CASTALIA (menu)
```

- **Kernel & shell:** the FreeDOS kernel (`KERNEL.SYS`) and FreeCOM
  (`COMMAND.COM`) — a legal, open-source, MS-DOS-compatible DOS.
- **Castalia layer:** ~20 original 16-bit real-mode programs, a shared text-mode
  UI toolkit, an INI parser, boot profiles, and configuration.
- **Filesystems:** FAT12 (floppy) and FAT16 (hard disk / CompactFlash).
- **Display:** VGA 80×25 text mode, 16 colours, drawn by direct video-memory
  writes for speed on a 386SX; BIOS for cursor and INT 16h for the keyboard.

## What we used to make it

| Piece | Tool / component |
|---|---|
| DOS kernel & shell | **FreeDOS** (kernel build 2043, FreeCOM 0.85a) — GPLv2+ |
| Memory managers, base utilities | FreeDOS HIMEMX, JEMM386, MEM, FDISK, FORMAT, SYS, XCOPY, CHKDSK, SHSUCDX, CTMOUSE, UIDE — open/free |
| C compiler / linker (16-bit) | **Open Watcom V2** (`wcc` / `wlink` / `wmake`) |
| Assembler (kernel rebuild) | **NASM** |
| Floppy image tooling | **mtools** (`mformat` / `mcopy`) + `dd` |
| Continuous integration | **GitHub Actions** |
| Emulated boot/smoke testing | **DOSBox** (headless) |
| Language | **C89**, real-mode, large model, no dynamic allocation |

The whole Castalia layer is deliberately old-school C89 with no heap use, so it
compiles small and loads fast on period hardware.

## The boot-banner rebuild (a notable detail)

The stock FreeDOS `KERNEL.SYS` prints a "FreeDOS kernel … Open Watcom …" sign-on
plus a licence block, unconditionally, from a packed (compressed) image whose
strings cannot be edited in place. To make the boot read *CASTALIA DOS* only,
the kernel is **rebuilt from its own published source** with a single, marked
change to the sign-on routine, and shipped uncompressed. This is a normal
white-label step that FreeDOS explicitly permits; FreeDOS's copyright, licence,
and warranty are retained in the shipped source, in the licence tree, and on the
system's own About screen.

## How it's built and verified (CI)

Every change runs an automated pipeline:

1. **Repository gate** — C89 syntax check of every source with a host compiler,
   shell-script linting, and repository sanity checks.
2. **Host unit tests** — the real shared modules (INI parser, the boot-banner
   rebrand tool) compiled and executed natively.
3. **16-bit DOS build** — all ~20 tools built for real with Open Watcom, verified
   as genuine DOS `MZ` executables; the FreeDOS kernel rebuilt with the CASTALIA
   sign-on.
4. **End-to-end boot test** — a FreeDOS payload is fetched, a bootable 1.44 MB
   floppy image is assembled, and it is **actually booted in a headless
   emulator**, asserting the system reaches `AUTOEXEC.BAT` from cold.

In other words: the project doesn't just compile — on every commit, a real disk
image boots in an emulator before the change is considered good.

## Requirements (minimum → comfortable)

- **CPU:** 386SX (minimum) → 486/Pentium (comfortable).
- **RAM:** works within the classic 640 KB conventional limit; XMS/UMB/EMS used
  where a memory manager is present.
- **Storage:** 1.44 MB floppy to boot/install; a formatted FAT16 hard disk or
  CompactFlash card to install onto.
- **Video:** VGA (16-colour text).
- **Optional:** serial or PS/2 mouse; AdLib/Sound Blaster-family audio; IDE
  CD-ROM.

## Repository layout (for reviewers)

- `src/` — the Castalia programs and shared `common/` toolkit.
- `config/` — the eight-profile `CONFIG.SYS`, `AUTOEXEC.BAT`, and `.INI` files.
- `scripts/` — payload fetch, kernel rebuild, floppy build, and test scripts.
- `docs/` — the project's technical "bible" (architecture, memory, branding,
  licensing, roadmap).
- `third_party/` — GPL FreeDOS components and their source (kept separate).
- `LICENSES/` — the full licence texts.

Source and full documentation: **https://github.com/davabe/Castalia-DOS**
