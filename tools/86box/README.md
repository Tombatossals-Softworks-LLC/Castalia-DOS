# tools/86box — accurate 386SX/486 emulation profiles

[86Box](https://86box.net/) (and PCem) emulate period hardware cycle-accurately,
which DOSBox-X does not. Use 86Box to validate real 386SX timing, memory
behaviour, and driver interaction before touching physical hardware. This is the
emulator the `EMU-*` rows in [`../../docs/TESTING.md`](../../docs/TESTING.md)
refer to.

86Box stores its settings in a per-VM `86box.cfg` that is tied to a specific
86Box build and ROM set, so a raw `.cfg` checked into the repo would break
across versions. Instead, this file documents the **machine settings** to
select in 86Box's GUI. Create one VM per row below.

## Recommended test machines

| Profile id | Machine (86Box) | CPU | RAM | Video | Notes |
|---|---|---|---|---|---|
| `castalia-386sx` | Generic 386SX board (e.g. AMI 386SX clone) | 386SX/16 or /25 | 4 MB | Generic VGA (Tseng ET4000 or S3) | The primary target. Expect games to match the 386SX column in COMPATIBILITY.md. |
| `castalia-386dx` | Generic 386DX board | 386DX/40 | 8 MB | ET4000 VGA | Faster bus; FPU optional (387). |
| `castalia-486`   | Generic 486 board (e.g. SiS 471) | 486DX2/66 | 16 MB | S3 Trio64 | Comfortable for Doom-era titles. |
| `castalia-pentium` | Socket 5/7 board | Pentium 100 | 32 MB | S3 Trio64 | Upper bound of the target range. |

## Common settings for every profile

- **Storage:** an IDE hard disk (or a small IDE/CF-style disk) plus a
  3.5″ 1.44 MB floppy drive. Attach the built floppy image
  (`dist/castalia-dos-*-boot.img`) to floppy A: to boot the installer/rescue
  disk; install to the IDE disk with `SETUP.EXE`.
- **Sound:** Sound Blaster Pro or Sound Blaster 16 at the default
  A220 I5 D1 (H5 for SB16) so it matches Castalia's `SET BLASTER` default.
- **Mouse:** a Microsoft-compatible serial mouse on COM1, or PS/2, to test
  `CTMOUSE`.
- **CD-ROM:** an ATAPI CD-ROM to test the `CDROM` profile (`UIDE` + `SHSUCDX`).

## What to verify per machine

Run the relevant rows from [`../../docs/TESTING.md`](../../docs/TESTING.md):

- **Boot:** each of the eight profiles boots to the expected place.
- **Memory:** `MEM /C /P` free-conventional figures match the targets in
  [`../../docs/MEMORY.md`](../../docs/MEMORY.md) (this is where 86Box's accuracy
  matters — DOSBox-X will not reproduce the exact UMB layout).
- **Timing-sensitive titles:** confirm the 386SX verdicts in
  [`../../docs/COMPATIBILITY.md`](../../docs/COMPATIBILITY.md) (e.g. Doom is a
  slideshow on `castalia-386sx`, smooth on `castalia-486`).
- **Install:** `SETUP.EXE` from the floppy to the IDE disk, then boot the disk.

## Why not a checked-in .cfg?

86Box `.cfg` files reference absolute ROM paths, disk image paths, and
build-specific option keys. Documenting the GUI settings is portable across
86Box versions and across contributors' machines; a stale binary `.cfg` is not.
If your team pins a single 86Box build, you can add exported `.cfg` files here
under that build's version, one per profile id above.
