# third_party/

Vendored third-party components that ship with CASTALIA DOS, kept separate from
the original Castalia code in `src/` for clarity and license hygiene.

These are **not** committed as binaries in this design repo; they are gathered
here at build time from their upstream sources. Each must arrive with its
license text and, for GPL components, its **source**, which is then reflected in
[`../LICENSES/`](../LICENSES/).

Expected contents when assembling a release:

| Component | Role | License | Source obligation |
|---|---|---|---|
| FreeDOS kernel (`KERNEL.SYS`) | DOS kernel | GPLv2+ | ship source on media |
| FreeCOM (`COMMAND.COM`) | shell | GPLv2+ | ship source on media |
| HIMEMX | XMS driver | open | include license |
| JEMM386 | EMM386-compatible | open | include license |
| CTMOUSE | mouse driver | **GPL** (verified from the package) | ship source — `SOURCE/CTMOUSE/SOURCES.ZIP` |
| UIDE / UDVD2 | IDE/ATAPI CD driver | open/free | include license |
| SHSUCDX | MSCDEX replacement | open/free | include license |
| KEYB, SETVER, MEM, FDISK, FORMAT, SYS, XCOPY, EDIT | base utilities | GPL/open | per component |
| SMARTDRV-style cache | disk cache | open | include license |

The populated per-component manifest — version, upstream, license, path, and
source obligation for everything actually vendored — lives at
[`../LICENSES/MANIFEST.md`](../LICENSES/MANIFEST.md), the location
[`../LICENSES/README.md`](../LICENSES/README.md) defines. (Section 21.5 of the
roadmap calls it `third_party/MANIFEST.md`; there is one manifest, and this is
the pointer to it.) Checksums are not hand-copied: `scripts/fetch-payload.sh`
writes `floppy/payload/PROVENANCE.txt` with the SHA256, size and URL of every
archive it downloads, so the record describes what was actually fetched.

See [`../docs/LICENSE-STRATEGY.md`](../docs/LICENSE-STRATEGY.md) for the full
policy and the release gate.
