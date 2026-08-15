# tools/

Host-side helper tools and emulator configuration used to build and test
CASTALIA DOS on a modern development machine. These run on the *host*, not on
the target DOS system.

Contents:

- [`dosbox-x.conf`](dosbox-x.conf) — a DOSBox-X profile for fast iteration on
  the Castalia tools (386 core, 4 MB, VGA, SB Pro, DOS 6.22 reporting). Run
  `dosbox-x -conf tools/dosbox-x.conf`.
- [`86box/README.md`](86box/README.md) — the recommended 86Box machine settings
  for the four reference machines (386SX/16, 386DX/40, 486DX2/66, Pentium 100)
  used by the `EMU-*` rows in [`../docs/TESTING.md`](../docs/TESTING.md).

Planned:

- Image-building helpers that will grow out of
  [`../scripts/build-floppy.md`](../scripts/build-floppy.md) and
  [`../scripts/build-floppy.sh`](../scripts/build-floppy.sh).

Nothing here ships on the CASTALIA DOS media.
