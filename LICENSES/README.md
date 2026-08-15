# LICENSES

This directory collects the license texts and a manifest for every component
distributed with CASTALIA DOS. It exists so that any release image can be
audited for license compliance, and so that GPL obligations (source
availability, license inclusion) are met on the media itself.

The licensing *strategy* — reasoning, obligations, and the release gate — is in
[`../docs/LICENSE-STRATEGY.md`](../docs/LICENSE-STRATEGY.md). This file is the
practical index.

## Layering summary

| Layer | Lives in | License | Copyleft? | Source shipped |
|---|---|---|---|---|
| Original Castalia code | `src/`, `build/` (as binaries) | MIT | no | yes (this repo) |
| Castalia documentation | `docs/` | CC BY 4.0 | no (share-alike-free) | yes |
| Castalia branding/artwork | `docs/BRANDING.md`, assets | proprietary marks / permissive art | — | n/a |
| Game database | `config/GAMES.INI` | CC0 (data) | no | yes |
| FreeDOS kernel/shell + GPL utils | `third_party/` | GPLv2+ | yes | **required** |
| Mouse driver (CTMOUSE) | `third_party/` | **GPL** (verified from the package; earlier notes said BSD-2) | yes | **required** |
| CD stack (UIDE/SHSUCDX), HIMEMX, JEMM386 | `third_party/` | open/free (see manifest) | mostly no | provided where required |

## Files expected in this directory

```
LICENSES/
├── README.md            this index
├── MANIFEST.md          per-component: name, version, license, upstream, path
├── GPLv2.txt            the GNU GPL v2 (kernel, FreeCOM, CTMOUSE, base utils)
├── Artistic-1.0.txt     JEMM386 (partly), and HimemX's alternative
├── InfoZIP.txt          Info-ZIP UNZIP/ZIP
├── CWSDPMI.txt          the DPMI host, GPLv2 + an additional permission
├── CC-BY-4.0.txt        documentation license
├── CC0-1.0.txt          data (GAMES.INI) dedication
└── THIRD-PARTY.txt      the notice file copied onto release media
```

This directory is now complete. The licence texts are copied **byte-for-byte**
from the packages they govern, or from the licence steward
(`creativecommons.org`) for the two CC texts; `.gitattributes` keeps
`LICENSES/*.txt` out of LF normalisation, because CWSDPMI's licence requires
verbatim distribution. The repository's root [`../LICENSE`](../LICENSE) (MIT)
governs the original Castalia source in `src/`.

## MANIFEST.md schema

Each vendored component gets one row. The rows below are the *shape*; the real,
verified table is in [`MANIFEST.md`](MANIFEST.md) and takes precedence over this
illustration.

| Component | Version | Upstream | License | Path in dist | Source obligation |
|---|---|---|---|---|---|
| FreeDOS kernel | 1.3 | freedos.org | GPLv2+ | `C:\` | ship source on media |
| FreeCOM | (bundled) | freedos.org | GPLv2+ | `C:\` | ship source on media |
| HIMEMX | (bundled) | (open) | open | `C:\DOS` | include license |
| JEMM386 | (bundled) | (open) | open | `C:\DOS` | include license |
| CTMOUSE | (bundled) | (open) | GPL | `C:\CASTALIA\DRV` | ship source |
| UIDE / SHSUCDX | (bundled) | (open/free) | open/free | `C:\DOS`, `DRV` | include license |

## Release gate (short form)

Before publishing any image, a maintainer confirms:

1. Every binary on the image has a row in `MANIFEST.md`.
2. Every GPL component's **source** is included on the media (or a written
   offer is present) per GPLv2 §3.
3. `THIRD-PARTY.txt` is present on the image and lists all licenses.
4. No Microsoft (or other proprietary) file has slipped into the payload.

The full 10-step gate is in
[`../docs/LICENSE-STRATEGY.md`](../docs/LICENSE-STRATEGY.md).
