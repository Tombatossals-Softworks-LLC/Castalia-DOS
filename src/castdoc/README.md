# src/castdoc — CASTDOC.EXE (prototype)

The Castalia disk doctor, surface side (`wmake castdoc`). **Read-only** by
construction: only INT 13h `AH=04h` (verify) is issued — it never writes.

Per drive (A:, B:, first hard disk): reports BIOS geometry and the floppy
type (360K/1.2M/720K/1.44M — distinguishes 5.25″ from 3.5″ drives), then
verifies the surface track by track with reset+retry, painting a live map
(green = OK, red `B` = errors). Floppies get a **full** scan; hard disks
default to a clearly-labelled **sampled** scan (`F` for full). Afterwards:
an error-code breakdown (CRC, address mark, seek, timeout…) and plain
advice on the real question — failing **diskette** or failing **drive**
(scan a known-good disk in the same drive to split the two; rescue files
with CASTCOPY). FAT/logical repair intentionally stays with FreeDOS CHKDSK.

Validation note: INT 13h behaviour must be exercised in 86Box and on real
hardware (the gcc gate only checks syntax). Links `ui` only.
