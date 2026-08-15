# scripts/build-floppy.md — Building the CASTALIA DOS boot/rescue floppy

This is the step-by-step recipe for building the CASTALIA DOS bootable 1.44 MB
floppy image (`castalia-boot.img`) on a modern Linux/macOS host, using only
free, open tools. The same image is the installer's first disk and the
emergency rescue disk. A Windows recipe (using the FreeDOS tools + `imdisk`)
follows at the end.

## Quick start (scripted)

The manual steps below are automated by
[`build-floppy.sh`](build-floppy.sh). The usual flow:

```sh
# 1. Gather the FreeDOS payload into floppy/payload/ and the FreeDOS
#    floppy boot sector into floppy/fdboot.bin (see below for what).
# 2. Check that everything needed is present (no image is written):
scripts/build-floppy.sh --manifest

# 3. Build the bootable image (builds the tools with wmake, then the image):
scripts/build-floppy.sh -v 0.2 -c peniscola

# Variations:
scripts/build-floppy.sh --no-tools     # use existing build/*.exe
scripts/build-floppy.sh --staging      # image without a boot sector
scripts/build-floppy.sh --help
```

`--manifest` uses only coreutils and prints a present/missing table for every
file the image needs, so you can confirm the payload is complete before
running a real build. The full build needs `mtools` + `dd` (image) and Open
Watcom `wmake` (tools). The rest of this document explains each step the
script performs, and remains the reference for doing it by hand.

## Prerequisites (Linux/macOS host)

| Tool | Package | Used for |
|---|---|---|
| `mtools` (`mformat`, `mcopy`, `mmd`) | `mtools` | create/populate a FAT12 image without mounting |
| `dd` | coreutils | create the blank image |
| Open Watcom V2 | (from open-watcom.github.io) | build the Castalia `.exe` tools |
| FreeDOS boot files | FreeDOS 1.3 install/floppy edition | `KERNEL.SYS`, `COMMAND.COM`, boot sector, base utils |

You also need, gathered into `floppy/payload/`:

- FreeDOS `KERNEL.SYS`, `COMMAND.COM`
- Memory managers `HIMEMX.EXE`, `JEMM386.EXE`
- Base utilities: `MEM.EXE`, `FDISK.EXE`, `FORMAT.EXE`, `SYS.COM`, `CHKDSK.EXE`,
  `XCOPY.EXE`, an editor (`EDIT`)
- Castalia tools built into `build/`: `CASTALIA.EXE`, `LAUNCH.EXE`,
  `HWINFO.EXE`, `SAFEBOOT.EXE`, `CFGEDIT.EXE`, `SETUP.EXE` (as available)
- Castalia config: `config/CONFIG.SYS`, `config/AUTOEXEC.BAT`,
  `config/*.INI`, `config/*.BAT`

> **License note:** `KERNEL.SYS` and `COMMAND.COM` are GPL FreeDOS binaries.
> Ship their source alongside the image (see
> [`../docs/LICENSE-STRATEGY.md`](../docs/LICENSE-STRATEGY.md)). Everything under
> `C:\CASTALIA` is original MIT-licensed Castalia work.

## Step 1 — Build the Castalia tools

```sh
# From the repo root, with the Open Watcom environment loaded:
wmake
# -> build/launch.exe, build/castalia.exe (and others as they are added)
```

## Step 2 — Create a blank 1.44 MB image

```sh
mkdir -p dist
dd if=/dev/zero of=dist/castalia-boot.img bs=512 count=2880
```

## Step 3 — Format it FAT12 and make it a FreeDOS boot disk

`mformat` writes a FAT12 filesystem; `-B` installs a boot sector. Use the
FreeDOS floppy boot sector (`fdboot.bin` from the FreeDOS boot media), which
loads `KERNEL.SYS`:

```sh
mformat -i dist/castalia-boot.img -f 1440 -B floppy/fdboot.bin ::
```

If you do not have `fdboot.bin`, an alternative is to boot a FreeDOS floppy in
an emulator and run `SYS A:` against the image; the `mformat -B` route is
preferred because it is scriptable.

## Step 4 — Create the directory layout on the image

```sh
mmd -i dist/castalia-boot.img ::/DOS
mmd -i dist/castalia-boot.img ::/CASTALIA
mmd -i dist/castalia-boot.img ::/CASTALIA/BIN
mmd -i dist/castalia-boot.img ::/CASTALIA/DRV
mmd -i dist/castalia-boot.img ::/CASTALIA/CFG
mmd -i dist/castalia-boot.img ::/CASTALIA/HELP
```

## Step 5 — Copy the kernel, shell, and config to the root

```sh
mcopy -i dist/castalia-boot.img floppy/payload/KERNEL.SYS   ::/
mcopy -i dist/castalia-boot.img floppy/payload/COMMAND.COM  ::/
mcopy -i dist/castalia-boot.img config/CONFIG.SYS           ::/
mcopy -i dist/castalia-boot.img config/AUTOEXEC.BAT         ::/
```

## Step 6 — Copy the DOS core, drivers, tools, and config

```sh
# DOS core
mcopy -i dist/castalia-boot.img floppy/payload/HIMEMX.EXE   ::/DOS/
mcopy -i dist/castalia-boot.img floppy/payload/JEMM386.EXE  ::/DOS/
mcopy -i dist/castalia-boot.img floppy/payload/MEM.EXE      ::/DOS/
mcopy -i dist/castalia-boot.img floppy/payload/FDISK.EXE    ::/DOS/
mcopy -i dist/castalia-boot.img floppy/payload/FORMAT.EXE   ::/DOS/
mcopy -i dist/castalia-boot.img floppy/payload/SYS.COM      ::/DOS/

# Castalia tools
mcopy -i dist/castalia-boot.img build/castalia.exe          ::/CASTALIA/BIN/
mcopy -i dist/castalia-boot.img build/launch.exe            ::/CASTALIA/BIN/
mcopy -i dist/castalia-boot.img config/GAMES.BAT            ::/CASTALIA/BIN/

# Config
mcopy -i dist/castalia-boot.img config/CASTALIA.INI         ::/CASTALIA/CFG/
mcopy -i dist/castalia-boot.img config/PROFILES.INI         ::/CASTALIA/CFG/
mcopy -i dist/castalia-boot.img config/GAMES.INI            ::/CASTALIA/CFG/
mcopy -i dist/castalia-boot.img config/SOUND.BAT            ::/CASTALIA/CFG/
```

(Add drivers and remaining tools with the same pattern as they are built.)

## Step 7 — Verify the contents

```sh
mdir -i dist/castalia-boot.img ::/
mdir -i dist/castalia-boot.img ::/CASTALIA/BIN
```

## Step 8 — Test in an emulator

```sh
# DOSBox-X (fast iteration):
dosbox-x -conf tools/dosbox-x.conf dist/castalia-boot.img

# 86Box / PCem (accurate 386SX): attach dist/castalia-boot.img as floppy A:
# and boot. See docs/TESTING.md for the emulator profiles.
```

Confirm: the Castalia boot menu appears, XMS is the default, the menu profiles
boot, `MEM /C` shows the expected free conventional memory, and
`CASTALIA.EXE` starts.

## Step 9 — Checksums and naming

```sh
cd dist
mv castalia-boot.img castalia-dos-0.2-peniscola-boot.img
sha256sum castalia-dos-0.2-peniscola-boot.img > \
          castalia-dos-0.2-peniscola-boot.img.sha256
```

See [`../docs/DISTRIBUTION.md`](../docs/DISTRIBUTION.md) for the full artifact
naming scheme and the multi-floppy / ZIP / ISO / CF variants.

---

## Windows host alternative

1. Build the tools with Open Watcom (`wmake`).
2. Create a blank 1.44 MB image and mount it writable with **ImDisk**, or use
   the FreeDOS **RUFUS**/**unetbootin** path for physical disks.
3. From a FreeDOS VM, run `SYS A:` (or `SYS <image>`), then `XCOPY` the payload
   and config trees onto it.
4. Test in DOSBox-X / 86Box exactly as above.

The Linux `mtools` route is preferred for CI because it needs no mounting and
no privileges, so it drops straight into `scripts/build-floppy.sh` later.
