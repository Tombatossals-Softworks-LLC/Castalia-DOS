/* ===================================================================
 * CRC32.H  -  CRC-32 (IEEE 802.3, the ZIP polynomial) for CASTALIA DOS
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * The catalogue checks every download against the size and CRC-32 its
 * index declares before anything is unpacked.  CRC-32 is what ZIP
 * itself uses, so `crc32` on the host and this module on a 386SX agree
 * on the same file; it is cheap enough to run over a few megabytes on
 * a 16 MHz machine, which a cryptographic hash is not.
 *
 * It detects a damaged or truncated transfer.  It does not prove where
 * a file came from: see docs/NETWORK.md, "What the checksum is for".
 *
 * Table-driven, the table built once on first use (1 KB).  C89, no
 * allocation, portable: the host unit tests compile this very file.
 * =================================================================== */

#ifndef CASTALIA_CRC32_H
#define CASTALIA_CRC32_H

#define CRC32_INIT  0xFFFFFFFFUL

/* Fold 'len' bytes into a running CRC.  Start from CRC32_INIT and pass
 * the result through crc32_final() once all the data is in. */
unsigned long crc32_update(unsigned long crc, const unsigned char *buf,
                           unsigned len);

/* The finished value, as `crc32` and ZIP print it. */
unsigned long crc32_final(unsigned long crc);

/* Called every 'chunk' while a file is checked, with the bytes done so
 * far, so a slow machine can draw progress.  May be NULL. */
typedef void (*crc32_progress)(unsigned long done);

/* CRC-32 and length of a whole file.  0 on success, -1 if it cannot be
 * opened, -2 on a read error. */
int crc32_file(const char *path, unsigned long *crc, unsigned long *size,
               crc32_progress progress);

#endif /* CASTALIA_CRC32_H */
