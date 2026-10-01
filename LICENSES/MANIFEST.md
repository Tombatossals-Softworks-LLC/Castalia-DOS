# MANIFEST — vendored components

Every third-party file that ships with CASTALIA DOS gets a row here, per the
schema in [`README.md`](README.md) and the Phase 0 requirement in
[`../docs/ROADMAP.md`](../docs/ROADMAP.md) that each component trace to an
upstream URL, a license, and a checksum.

Nothing in this table is committed to the repository. `scripts/fetch-payload.sh`
gathers it at build time and writes **`floppy/payload/PROVENANCE.txt`** —
one line per downloaded archive: `SHA256  filename  size  URL`. That file is
the checksum record, and it describes what was *actually* fetched on that run
rather than what someone believed was fetched. Read it alongside this table.

## Core system

| Component | Version | Upstream | License | Path in dist | Source obligation |
|---|---|---|---|---|---|
| FreeDOS kernel (`KERNEL.SYS`) | 2043, rebuilt as the Castalia kernel | ibiblio `dos/kernel/2043/ke2043s.zip` | GPLv2+ — [`GPLv2.txt`](GPLv2.txt), taken from `COPYING` in that source archive | `C:\` | ship source on media; the *modified* source is produced by `scripts/patch-kernel-src.py`, see [`../docs/KERNEL.md`](../docs/KERNEL.md) |
| FAT12 boot sector (`fdboot.bin`) | copied from the FloppyEdition boot image; assembled from `boot/boot.asm` only on the package fallback route | ibiblio `FD13-FloppyEdition.zip`, or `ke2043s.zip` → `boot/boot.asm` | GPLv2+ | boot sector | `KERNEL-SRC.ZIP` in the sources archive (same source either way) |
| FreeCOM (`COMMAND.COM`) | FreeDOS 1.3 `freecom.zip` | ibiblio `repositories/1.3/base` | GPLv2+ | `C:\` | ship source on media |
| `SYS.COM` | FreeDOS 1.3 `kernel.zip` | ibiblio `repositories/1.3/base` | GPLv2+ | `C:\DOS` | ship source on media |
| `FDISK.EXE`, `FORMAT.EXE`, `MEM.EXE`, `XCOPY.EXE`, `CHKDSK.EXE` | FreeDOS 1.3 | ibiblio `repositories/1.3/base` | GPLv2+ | `C:\DOS` | ship source on media |
| `HIMEMX.EXE` | FreeDOS 1.3 `himemx.zip` | ibiblio `repositories/1.3/base` | **GPL and/or Artistic** — "All changes done for HimemX are Public Domain. However, since HimemX is derived from FD Himem, the FD Himem copyrights do apply. FD Himem is copyright Till Gerken and Tom Ehlert, with GPL and/or Artistic license." (`DOC/HIMEMX/README.TXT` §4) | `C:\DOS` | ship `SOURCE/HIMEMX/SOURCES.ZIP` from the package |
| `JEMM386.EXE` | FreeDOS 1.3 `jemm.zip` | ibiblio `repositories/1.3/base` | **Artistic License (partly)** — "JEMM386/JEMMEX: partly Artistic License" (`DOC/JEMM/README.TXT` §8); the bundled tools are Public Domain. Text: [`Artistic-1.0.txt`](Artistic-1.0.txt) | `C:\DOS` | ship `SOURCE/JEMM/SOURCES.ZIP` from the package |
| `SHSUCDX.COM` | 3.07 | ibiblio `repositories/1.3/base` | **Freeware**, © 2006-2020 Jason Hood (`DOC/SHSUCDX/SHSUCDX.TXT`) | `C:\DOS` | source ships in the package (`SOURCE/SHSUCDX/SOURCES.ZIP`); carry it |
| `CTMOUSE.EXE` | CuteMouse 2.x | ibiblio `repositories/1.3/base` | **GPL** — "CuteMouse is released under the terms of the GNU General Public License (GPL)", © 1997-2002 Nagy Daniel (`DOC/CTMOUSE/CTMOUSE.TXT`). **Not BSD-2-Clause**, which earlier project notes assumed | `C:\CASTALIA\DRV` | **required** — ship `SOURCE/CTMOUSE/SOURCES.ZIP` from the package |
| `UIDE.SYS` | FreeDOS 1.3 `uide.zip` | ibiblio `repositories/1.3/drivers` | "Open Source DOS device drivers" (`DOC/UIDE/README.TXT` §1); source ships with the package | `C:\CASTALIA\DRV` | ship `SOURCE/UIDE/SOURCES.ZIP` from the package |

> Every licence above was read out of the shipped package, not carried over
> from project notes. That exercise corrected one thing: **CTMOUSE is GPL, not
> BSD-2-Clause** as `README.md` and `../third_party/README.md` had recorded, so
> its source is a *requirement* rather than a courtesy. Helpfully, every FreeDOS
> package here ships its own `SOURCE/<NAME>/SOURCES.ZIP`, so the obligation is
> met by carrying those files — `fetch-payload.sh --with-sources` stages them.

## Archive tools (opt-in: `fetch-payload.sh --with-archiver`)

These answer the "PKZIP / ARJ / LHA" row in [`../docs/APPS.md`](../docs/APPS.md).
They are **not** on the 1.44 MB rescue floppy — `UNZIP.EXE` alone is 193 KB and
the floppy already carries the kernel, the shell, the DOS utilities, the whole
Castalia suite and the help pages — so they are staged in
`floppy/payload/ARCHIVER/` for the hard-disk distribution.

| Component | Version | Upstream | License | Path in dist | Source obligation |
|---|---|---|---|---|---|
| Info-ZIP `UNZIP.EXE` | UnZip 6.00a | ibiblio `repositories/1.3/archiver/unzip.zip`; project at `infozip.sourceforge.net` | Info-ZIP (permissive) — [`InfoZIP.txt`](InfoZIP.txt), verified from `DOC/UNZIP/LICENSE` in the package | `C:\DOS` | include license; no source obligation |
| Info-ZIP `ZIP.EXE` | Zip 3.0 (the **16-bit** `ZIP16.EXE` build) | ibiblio `repositories/1.3/archiver/zip.zip` | Info-ZIP (permissive) — [`InfoZIP.txt`](InfoZIP.txt) | `C:\DOS` | include license; no source obligation |
| `CWSDPMI.EXE` | CWSDPMI r7a | ibiblio `repositories/1.3/util/cwsdpmi.zip`; project at `sandmann.dotster.com/cwsdpmi/` | GPLv2 **with an additional permission** — [`CWSDPMI.txt`](CWSDPMI.txt), verified from `DOC/CWSDPMI/COPYING.CWS` | `C:\DOS` | see below |

### Why a DPMI host is in this list

FreeDOS's `UNZIP.EXE` is a **DJGPP build**: it carries the `go32` stub and needs
a DPMI host, so on a bare real-mode 386SX it will not run alone. There is no
16-bit UnZip in the FreeDOS repository — the 1.2 package ships the byte-identical
binary — so `CWSDPMI.EXE` is vendored alongside it, which a DJGPP program loads
from the `PATH` by itself. `ZIP` is different: upstream ships both, and Castalia
takes `ZIP16.EXE`, the real-mode build, so *creating* archives needs no DPMI host
at all. `fetch-payload.sh` asserts both facts (it fails if `ZIP.EXE` turns out to
carry the DJGPP stub, or if the DPMI host is missing next to `UNZIP.EXE`).

### The CWSDPMI obligation, precisely

CWSDPMI's source is GPLv2, but its author grants an explicit additional
permission, quoted verbatim in [`CWSDPMI.txt`](CWSDPMI.txt): the *official,
unmodified* `CWSDPMI.EXE` binary may be distributed together with **precise
information as to where to obtain the source code on the Internet**, in place of
the source or written offer that GPLv2 §3 would otherwise require. Castalia ships
the official binary unmodified, so the obligation this project must meet is to
carry that pointer on the media:

> CWSDPMI source: `http://sandmann.dotster.com/cwsdpmi/`

That line belongs in `THIRD-PARTY.txt` on any release image that includes the
archive tools. All other GPL terms continue to apply.

## Release gate

The short gate is in [`README.md`](README.md) and the full one in
[`../docs/LICENSE-STRATEGY.md`](../docs/LICENSE-STRATEGY.md). Additions this
manifest implies:

1. Check `floppy/payload/PROVENANCE.txt` from the release build — every row
   above should have a matching downloaded archive. Every archive is also
   checked against the digest pinned in `scripts/payload.sha256`; the build
   stops on a mismatch.
2. Run `fetch-payload.sh --with-sources` for a release build. It stages a
   `SOURCES.ZIP` for every GPL/Artistic component in the table above and
   fails if one is missing. `build-floppy.sh` then writes
   `castalia-dos-<version>-<codename>-sources.zip` beside the image.
3. Publish that sources archive from the same place as the image. The source
   does not fit on a 1.44 MB disk; GPLv2 §3 accepts equivalent access from
   the same place, and `THIRD-PARTY.txt` on the disk points to the archive.
4. Keep the CWSDPMI source URL quoted above in `THIRD-PARTY.txt` whenever the
   archive tools are included.
