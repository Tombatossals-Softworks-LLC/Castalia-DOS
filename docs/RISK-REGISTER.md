# 22. Risk Register

**Document:** CASTALIA DOS — Technical Bible, Section 22
**Applies to:** CASTALIA DOS 386SX Edition (all releases through 1.0 "Tombatossals")
**Status:** Living document — reviewed every milestone
**License:** CC BY 4.0 (documentation)
**Owner of this section:** Project Engineering Lead (`davabe`)

---

## 22.1 Purpose and scope

This register enumerates the engineering, legal, and operational risks that
threaten CASTALIA DOS as a serious, legally-clean, FreeDOS-based, game-focused
DOS environment for real 386-class hardware. Each risk carries a severity, a
probability, a concrete and actionable mitigation, and a realistic detection
method tied to our test IDs, CI checks, or user-report channels.

The register exists to make trade-offs explicit. CASTALIA DOS holds one guiding
rule above elegance: **compatibility beats elegance**. Many risks below are
accepted deliberately because the alternative (dropping a game, a memory
profile, or a class of hardware) would hurt that goal more than the risk does.
Where a risk is *accepted* rather than *eliminated*, the mitigation says so.

Scope: risks that affect shipped behavior on target hardware (386SX, 386DX,
486, early Pentium), the installer, the licensing posture, and the release and
distribution pipeline. Out of scope: business/marketing risk, and any risk that
presumes Microsoft-owned code (we never ship it, so it never enters our chain).

---

## 22.2 How to read this register

### Severity (impact if it occurs)

| Level    | Meaning                                                              |
|----------|----------------------------------------------------------------------|
| Low      | Cosmetic or single-title annoyance; easy user workaround.            |
| Med      | A feature, profile, or common title is degraded; workaround exists.  |
| High     | A core promise (compatibility, boot, memory) fails for many users.   |
| Critical | Data loss, legal exposure, or an unbootable/unshippable release.     |

### Probability (likelihood over the 0.x → 1.0 window, untreated)

| Level | Meaning                                                     |
|-------|-------------------------------------------------------------|
| Low   | Plausible but not expected without a specific trigger.      |
| Med   | Expected to occur at least once across supported hardware.  |
| High  | Effectively certain to occur in normal use/testing.         |

### Exposure score

Exposure = Severity weight (Low 1, Med 2, High 3, Critical 4) × Probability
weight (Low 1, Med 2, High 3). Used only to order the watchlist and heat
summary; it is not a column in the tables. Range 1–12.

---

## 22.3 Detection-method ID conventions

So that the "Detection method" column is unambiguous, CASTALIA DOS uses these
identifier patterns throughout the bible and the `tests/` tree:

| Prefix        | Meaning                                                        | Lives in / raised by                     |
|---------------|----------------------------------------------------------------|------------------------------------------|
| `TC-<AREA>-n` | Scripted or manual test case (regression matrix).              | `tests/` (e.g. `TC-CMP-101`)             |
| `HW-LAB-n`    | Real-hardware validation run on a lab machine.                 | Hardware lab logbook                      |
| `INST-CHK-n`  | Install-time guard performed by `SETUP.EXE`.                    | `src/setup`                               |
| `ci/<name>`   | Continuous-integration gate on every PR/tag.                   | CI pipeline (`scripts/`, `build/`)        |
| `compat-report` | Structured user game report (GitHub issue template).         | Issue tracker / `GAMES.INI` status field  |
| `bug` / `field` | Free-form user bug report or field telemetry.                | Issue tracker / `HWINFO.LOG`              |

Areas (`<AREA>`): `CMP` compatibility, `MEM` memory, `CD` CD-ROM, `SND` sound,
`INP` input, `WIN` Windows 3.x, `FS` filesystem/disk, `BOOT` boot/install,
`LIC` licensing, `HW` hardware, `DIST` distribution, `EMU` emulator smoke.

Diagnostics tooling: `HWINFO.EXE` writes `C:\CASTALIA\LOGS\HWINFO.LOG`;
`MEMPROF.EXE` has a `/CHECK` self-test; `SETUP.EXE` writes an install transcript
to `C:\CASTALIA\LOGS\SETUP.LOG`. Backups of the user's original `CONFIG.SYS` and
`AUTOEXEC.BAT` land in `C:\CASTALIA\BACKUP\`.

---

## 22.4 Master risk table

All risks, at a glance. Detailed, expanded entries follow by category in §22.7.

| Risk ID | Risk | Category | Severity | Probability | Impact | Mitigation | Detection method | Owner / Component |
|---------|------|----------|----------|-------------|--------|------------|------------------|-------------------|
| CMP-01 | Certain MS-DOS games misbehave, glitch, or refuse to run | Game Compatibility | High | High | Broken titles undercut the core game-focused promise | Curated `GAMES.INI` profiles + `GAMECFG.EXE` per-game overrides; ship known-good defaults; document workarounds | `TC-CMP-1xx` regression matrix; `compat-report`; `GAMES.INI` status field | Compatibility WG — `GAMES.INI` / `GAMECFG.EXE` |
| CMP-02 | SETVER / version reporting breaks version-sensitive titles | Game Compatibility | Med | Med | Games see kernel 7.x and abort or misbehave | Per-program `SETVER` table shipped pre-seeded; document that we spoof per-program, never globally | `TC-CMP-2xx` (version-gated titles); `compat-report` tagged `setver` | Compatibility WG — `SETVER` table |
| CMP-03 | VESA/SVGA (VBE) support gaps for later 90s titles | Game Compatibility | Med | Med | Hi-res/VBE 2.0 games fail or fall back to slow modes | Bundle a free VBE TSR for cards lacking VBE 2.0; per-game note in `GAMES.INI`; accept limits on 386SX-era chips | `TC-CMP-3xx` (VBE titles); `HWINFO.EXE` VBE probe; `compat-report` | Compatibility WG — video profile |
| MEM-01 | Memory-manager incompatibility (JEMM386/EMM386 vs specific titles) | Memory Management | High | High | EMS/UMB profile crashes or corrupts a game | Layered profiles (CLEAN has no EMM386); `MEMPROF.EXE` one-key fallback to CLEAN; EMM386 as alt to JEMM386 | `TC-MEM-1xx` per profile × title; `MEMPROF /CHECK`; `compat-report` | Memory subsystem — `MEMPROF.EXE` / `CONFIG.SYS` |
| MEM-02 | TSR load-order fragility / conventional-memory fragmentation | Memory Management | Med | Med | Drivers refuse to load high; free RAM below game minimum | Fixed, tested `LOADHIGH` order per profile; `MEM /C` targets; regression on free-KB thresholds | `TC-MEM-2xx` free-conventional assertions; `MEMPROF /CHECK` | Memory subsystem — profile authoring |
| MEM-03 | EMS page-frame collision with adapter ROM at E000 | Memory Management | Med | Med | EMS/CDROM/WIN3X profiles hang on some boards | Probe UMB/ROM regions before placing frame; configurable `FRAME=`; documented safe frames | `TC-MEM-3xx`; `HW-LAB` boot on adapter-heavy rigs; `HWINFO.EXE` UMB map | Memory subsystem — EMS profiles |
| PERF-01 | 386SX performance too low for some titles | Performance | Med | High | Playable-but-slow or unplayable on 16-bit-bus 386SX | Set honest minimums per game in `GAMES.INI`; prefer XMS profile for max conventional; document expectations | `HW-LAB-386SX` timing runs; `compat-report` "slow" flag | Perf/Kernel — profiles, tuning |
| FS-01 | FAT16 partition-size limits / CompactFlash CHS geometry | Filesystem & Data | High | Med | Wrong geometry yields unbootable or truncated volumes | `SETUP.EXE` reads BIOS/CF geometry, caps FAT16 ≤ 2 GB, warns on LBA/CHS mismatch | `INST-CHK-1x` geometry guard; `TC-FS-1xx`; `SETUP.LOG` | Installer — `SETUP.EXE` |
| FS-02 | Data loss when installing over an existing DOS system | Filesystem & Data | Critical | Med | User's files/config destroyed; trust lost | Non-destructive default; back up `CONFIG.SYS`/`AUTOEXEC.BAT` to `BACKUP\`; explicit typed confirm before any format | `INST-CHK-2x` destructive-op gate; `TC-BOOT-2xx`; `SETUP.LOG` | Installer — `SETUP.EXE` |
| FS-03 | Boot-sector installation failure (unbootable after install) | Filesystem & Data | High | Med | Machine won't boot; user stranded | Write/verify boot sector, keep pre-image backup, offer bootable-floppy rescue via `SAFEBOOT.EXE` | `INST-CHK-3x` verify-after-write; `TC-BOOT-1xx`; `HW-LAB` boot test | Installer — `SETUP.EXE` / boot code |
| FS-04 | SMARTDRV write-behind cache loses data on unclean shutdown | Filesystem & Data | High | Med | Save-games/config corrupted on reset or power-off | Default write-behind OFF (read cache only) except WIN3X; flush on menu exit; document safe power-off | `TC-FS-4xx` power-cut sim (86Box); `bug` reports | Perf/Cache — `SMARTDRV` config |
| CD-01 | CD-ROM driver complexity (UIDE/SHSUCDX quirks, drive-letter clash) | CD-ROM | Med | High | CD games can't find the drive; letters collide | Deterministic letter assignment; `SHSUCDX /L:`; CDROM profile isolates CD stack; documented order | `TC-CD-1xx`; `INST-CHK-4x` letter check; `compat-report` tagged `cdrom` | CD subsystem — `UIDE.SYS` / `SHSUCDX` |
| SND-01 | Sound Blaster auto-detection unreliable | Sound & Audio | Med | High | Wrong/absent audio; game "no sound card" errors | `SETSOUND.EXE` probes then lets user confirm; write explicit `SET BLASTER=`; manual override always available | `TC-SND-1xx` probe matrix; `HWINFO.EXE` audio probe; `bug` | Audio — `SETSOUND.EXE` |
| SND-02 | IRQ/DMA conflicts (sound vs CD vs NIC vs LPT) | Sound & Audio | High | Med | Lockups, no audio, or corrupt DMA transfers | `HWINFO.EXE` resource map flags overlaps; `SETSOUND.EXE` validates IRQ/DMA; documented safe defaults (A220 I5 D1 H5) | `TC-SND-2xx` conflict cases; `HWINFO.EXE` conflict warning; `HW-LAB` | Audio — `SETSOUND.EXE` / `HWINFO.EXE` |
| INP-01 | Mouse (CTMOUSE) detection / COM-port conflict | Input Devices | Low | Med | No mouse, or serial mouse steals a COM port | `CTMOUSE` auto-detect with manual `COMn` override; document serial-vs-PS/2; skip if absent | `TC-INP-1xx`; `HWINFO.EXE` mouse/COM probe; `bug` | Input — `CTMOUSE` config |
| INP-02 | Keyboard layout / KEYB / codepage mismatch | Input Devices | Low | Med | Wrong keys (ES vs US), missing accents | `KEYB` layout selectable at install; sane default; documented change procedure | `TC-INP-2xx`; `INST-CHK-5x` layout prompt; `bug` | Input — `KEYB` config |
| WIN-01 | Windows 3.x compatibility limitations | Windows 3.x | Med | Med | WfW/3.1 unstable or 386-enhanced mode fails | Dedicated `WIN3X` profile (EMS + UMB + SMARTDRV); document standard vs 386-enhanced limits; treat as secondary goal | `TC-WIN-1xx`; `HW-LAB` Win3.11 boot; `bug` | Windows integration — `WIN3X` profile |
| UX-01 | User confusion around boot profiles | User Experience | Med | High | Users pick wrong profile; games "don't work" | Clear boot menu labels; `MEMPROF.EXE` recommends per game; in-menu help; XMS as sensible default | `TC-CMP-1xx` w/ default profile; `compat-report` "wrong profile"; docs review | UX/Docs — `CASTALIA.EXE` / `help/` |
| UX-02 | Localization scope / mixed-language UI inconsistency | User Experience | Low | Med | Half-Spanish/half-English UI feels unfinished | Freeze 1.0 UI to English strings in one table; treat ES/Valencian as post-1.0; single string source | `ci/string-lint` (untranslated/dup keys); docs review | UX/Docs — `CASTALIA.EXE` strings |
| LIC-01 | GPL licensing mistake (static link, missing source offer) | Licensing & Legal | Critical | Med | License violation; must pull release | MIT tools talk to GPL components as separate EXEs/drivers — never static-link; ship GPL source in `third_party/`; SPDX headers | `ci/reuse-lint`, `ci/license-scan`, `ci/spdx-check`; release checklist | Legal lead — `LICENSES/` / `third_party/` |
| LIC-02 | Brand/trademark accidental infringement | Licensing & Legal | High | Low | Trademark/passing-off claim; forced rename | Never use Microsoft marks/branding/manual text; original Castalia branding only; legal name review pre-release | `ci/brand-scan` (banned-term grep); manual legal review | Legal lead — branding |
| LIC-03 | Contributor supply-chain: untrusted `third_party/` binaries | Licensing & Legal | High | Med | Malicious/opaque binary ships to users | Prefer build-from-source; record provenance + checksum + license for every artifact; two-person review | `ci/binary-provenance` (unpinned/unhashed blob fails); PR review | Legal/Release — `third_party/` |
| HW-01 | Real-hardware variance (BIOS quirks, chipset UMB regions) | Hardware & Platform | High | High | Works in emulator, fails on a specific board | Conservative defaults; probe don't assume; maintain tested-hardware matrix; `SAFE`/`CLEAN` fallbacks always present | `HW-LAB-*` matrix; `HWINFO.EXE` chipset/UMB map; `bug` w/ HWINFO.LOG | Hardware lab — `HWINFO.EXE` |
| QA-01 | Lack of automated testing on real hardware | Testing & QA | High | High | Regressions ship undetected to real 386SX users | Scheduled `HW-LAB` runs on a fixed rig set each milestone; checklist gating release; community field program | `HW-LAB-*` logbook; release gate; `field` reports | QA — `tests/` + lab |
| QA-02 | Emulator-only validation gives false confidence | Testing & QA | High | Med | Green in DOSBox-X/86Box but broken on metal | Two-tier: `TC-EMU-*` fast gate + mandatory `HW-LAB` before tagging; document emulator-vs-metal deltas | `ci/emu-smoke`; `HW-LAB` sign-off required to tag | QA — emulator + lab |
| DIST-01 | Floppy distribution complexity (bad sectors, 720K vs 1.44M, imaging) | Distribution & Build | Med | High | Corrupt/unreadable install media | Ship verified images for 720K and 1.44M; CRC each file; `RAWRITE`/`dd` docs; multi-disk set with per-disk verify | `ci/floppy-verify` (image geometry + CRC); `TC-DIST-1xx`; `bug` | Release eng — `floppy/` / `dist/` |
| DIST-02 | Build reproducibility / Open Watcom toolchain drift | Distribution & Build | Low | Med | Different builds from same source; hard to support | Pin Open Watcom version; `wmake` from clean tree in CI; record toolchain + hashes in release notes | `ci/build-watcom` pinned; artifact-hash compare | Release eng — `build/` / `scripts/` |

---

## 22.5 Severity × Probability heat summary

Cells list Risk IDs. Read top-right (High probability × Critical severity) as the
danger corner; nothing sits there today, but the High/High and Med/Critical
cells are the watchlist.

| Probability ↓ / Severity → | Low | Med | High | Critical |
|-----------------------------|-----|-----|------|----------|
| **High** | — | PERF-01, CD-01, SND-01, UX-01, DIST-01 | **CMP-01, MEM-01, HW-01, QA-01** | — |
| **Med**  | INP-01, INP-02, UX-02, DIST-02 | CMP-02, CMP-03, MEM-02, MEM-03, WIN-01 | LIC-03, FS-01, FS-03, FS-04, SND-02, QA-02 | **LIC-01, FS-02** |
| **Low**  | — | — | LIC-02 | — |

Exposure tally (Severity × Probability weights):

| Exposure | Risk IDs |
|----------|----------|
| 9 (top)  | CMP-01, MEM-01, HW-01, QA-01 |
| 8        | FS-02, LIC-01 |
| 6        | PERF-01, CD-01, SND-01, UX-01, DIST-01, LIC-03, FS-01, FS-03, FS-04, SND-02, QA-02 |
| 4        | CMP-02, CMP-03, MEM-02, MEM-03, WIN-01 |
| 3        | LIC-02 |
| 2        | INP-01, INP-02, UX-02, DIST-02 |

---

## 22.6 Top 5 risks to watch for 1.0 "Tombatossals"

These five carry the highest exposure *and* strike directly at the CASTALIA DOS
promise. HW-01 (real-hardware variance, exposure 9) trails immediately behind
and is effectively #6 — it is folded into QA-01's mitigation because our answer
to both is the same: test on metal.

1. **CMP-01 — Game compatibility (exposure 9).** This is the product. Treat the
   `GAMES.INI` compatibility database and `GAMECFG.EXE` as first-class 1.0
   deliverables, not afterthoughts. Ship a curated top-100 titles list with a
   verified status for each. *Gate:* no title regresses from OK to BROKEN in
   `TC-CMP-1xx` between release candidates.

2. **MEM-01 — Memory-manager incompatibility (exposure 9).** JEMM386/EMM386
   interactions are the deepest source of "it crashes only on my machine."
   Keep the CLEAN profile pristine (no EMM386) as the universal escape hatch,
   and make `MEMPROF.EXE` fall back to CLEAN in one keystroke. *Gate:* every
   top-100 title passes under at least one shipped profile on the lab rigs.

3. **FS-02 — Data loss on install over existing DOS (exposure 8).** One
   destroyed user disk is a reputation-ending event for a retro project.
   Non-destructive by default, mandatory backups to `C:\CASTALIA\BACKUP\`, and a
   typed confirmation before any format. *Gate:* `INST-CHK-2x` proves no write
   to an existing partition occurs without explicit confirmation.

4. **LIC-01 — GPL licensing mistake (exposure 8).** Existential and legal, not
   merely technical. The MIT/GPL boundary — Castalia tools as separate
   executables, never static-linked into GPL code — must be enforced by CI, not
   by memory. *Gate:* `ci/reuse-lint`, `ci/license-scan`, and `ci/spdx-check`
   are green and the source-offer for every `third_party/` GPL component is
   present in the release artifact.

5. **QA-01 — No automated real-hardware testing (exposure 9).** Everything
   above is only as trustworthy as our testing, and emulators lie (see QA-02,
   HW-01). Before tagging 1.0, a fixed matrix of real machines — at minimum one
   386SX, one 386DX, one 486, one early Pentium — must pass the release
   checklist and be signed off in the `HW-LAB` logbook. *Gate:* no tag without
   a completed `HW-LAB` sign-off.

---

## 22.7 Risks grouped by category (detailed)

The tables below expand the master rows with fuller mitigation and detection
detail. Same nine columns; use these when actually working a category.

### 22.7.1 Game Compatibility

CASTALIA DOS lives or dies here. Our defense is a data-driven compatibility
layer (`GAMES.INI` + `GAMECFG.EXE`) plus the layered boot profiles, so that a
troublesome title gets a targeted fix instead of a global compromise.

| Risk ID | Risk | Category | Severity | Probability | Impact | Mitigation | Detection method | Owner / Component |
|---------|------|----------|----------|-------------|--------|------------|------------------|-------------------|
| CMP-01 | Certain MS-DOS games glitch or refuse to run under our stack | Game Compatibility | High | High | Directly breaks the game-focused value proposition; erodes trust in the whole distro | Maintain a curated `GAMES.INI` with per-title recommended profile, `SET BLASTER=`, and quirk flags; `GAMECFG.EXE` for user overrides; ship verified defaults for a top-100 list; publish workarounds in `help/`; when a title needs a nonstandard profile, add it rather than change global defaults | `TC-CMP-1xx` regression matrix run each RC (title × profile); community `compat-report` template feeding the `GAMES.INI` status field (OK / WORKAROUND / BROKEN); trend tracked per release | Compatibility WG — `GAMES.INI` / `GAMECFG.EXE` |
| CMP-02 | Version-sensitive titles reject the FreeDOS kernel's 7.x report | Game Compatibility | Med | Med | Installers/games that gate on DOS 5.0/6.22 abort or misbehave | Pre-seed the `SETVER` table for known offenders (per-program spoof only); document clearly that we never claim a global 6.22 version, only 6.22-*compatible behavior*; expose a `GAMECFG.EXE` toggle to add a program's version | `TC-CMP-2xx` version-gated launch tests; `compat-report` tagged `setver`; `SETVER` table diff reviewed per release | Compatibility WG — `SETVER` table |
| CMP-03 | VBE/VESA gaps break hi-res or SVGA-era titles | Game Compatibility | Med | Med | Late-90s titles fail to init video or run in slow fallback modes | Bundle a free VBE TSR for cards without VBE 2.0; annotate affected titles in `GAMES.INI`; be honest that 386SX-era chipsets cap what is achievable and mark such titles accordingly | `TC-CMP-3xx` VBE-dependent titles; `HWINFO.EXE` VBE-version probe logged to `HWINFO.LOG`; `compat-report` tagged `video` | Compatibility WG — video profile |

### 22.7.2 Memory Management

The layered profiles (SAFE / CLEAN / XMS / EMS / CDROM / WIN3X) are the core
compatibility mechanism. CLEAN — HIMEMX + `DOS=HIGH`, no EMM386, no UMB — is the
deliberate universal fallback for cranky real-mode games.

| Risk ID | Risk | Category | Severity | Probability | Impact | Mitigation | Detection method | Owner / Component |
|---------|------|----------|----------|-------------|--------|------------|------------------|-------------------|
| MEM-01 | JEMM386/EMM386 interactions crash or corrupt specific titles | Memory Management | High | High | A game that needs a clean real-mode map hangs under EMS/UMB | Keep CLEAN free of any EMM386; make `MEMPROF.EXE` switch to CLEAN in one keystroke and recommend the right profile per title; keep FreeDOS `EMM386` available as an alternate to `JEMM386` for machines where one misbehaves | `TC-MEM-1xx` (each profile × representative titles); `MEMPROF /CHECK` self-test of the active map; `compat-report` tagged `memory` | Memory subsystem — `MEMPROF.EXE` / `CONFIG.SYS` |
| MEM-02 | Driver load-order/fragmentation drops free conventional below a title's minimum | Memory Management | Med | Med | Game reports "not enough memory" despite nominal free RAM | Fix and test the `LOADHIGH`/`DEVICEHIGH` order per profile; assert free-conventional thresholds (XMS target ~620–631 KB, CLEAN ~615 KB) in tests; keep TSRs minimal and loaded high | `TC-MEM-2xx` free-KB assertions per profile; `MEM /C` snapshots; `MEMPROF /CHECK` | Memory subsystem — profile authoring |
| MEM-03 | EMS page frame collides with adapter ROM around E000 | Memory Management | Med | Med | EMS/CDROM/WIN3X profiles hang at boot on some boards | Probe UMB/ROM regions before committing a frame; keep `FRAME=` configurable (default E000 with documented safe alternates); fall back to a NOEMS/XMS profile automatically if the frame can't be placed | `TC-MEM-3xx`; `HW-LAB` boot on adapter-heavy configs; `HWINFO.EXE` UMB/ROM map | Memory subsystem — EMS profiles |

### 22.7.3 Licensing & Legal

The MIT (Castalia) / GPLv2+ (FreeDOS) boundary is non-negotiable and must be
machine-enforced. We ship no Microsoft code, binaries, manual text, or branding.

| Risk ID | Risk | Category | Severity | Probability | Impact | Mitigation | Detection method | Owner / Component |
|---------|------|----------|----------|-------------|--------|------------|------------------|-------------------|
| LIC-01 | Accidental static link of GPL code into MIT tools, or a missing source offer | Licensing & Legal | Critical | Med | GPL violation; release must be withdrawn and re-cut | Architect Castalia tools to invoke FreeDOS components as separate executables/drivers — never static-link GPL objects; keep GPL components in `third_party/` with their source; SPDX headers on all files; a release checklist item confirms the corresponding source is bundled/offered | `ci/reuse-lint` + `ci/license-scan` + `ci/spdx-check` on every PR; release-gate checklist verifying source offer for each GPL artifact | Legal lead — `LICENSES/` / `third_party/` |
| LIC-02 | Accidental trademark/branding infringement | Licensing & Legal | High | Low | Passing-off or trademark claim; forced rename/rebrand | Use only original Castalia branding (Castilian-fortress identity); never reproduce Microsoft/other marks, logos, or manual wording; run a banned-term scan and a manual legal name review before each release | `ci/brand-scan` (grep for banned marks/terms in strings, docs, art metadata); manual legal review pre-tag | Legal lead — branding |
| LIC-03 | Untrusted third-party binaries enter the supply chain | Licensing & Legal | High | Med | A malicious or unauditable blob ships to users | Prefer build-from-source; for any prebuilt artifact record provenance URL, upstream version, license, and SHA-256; require two-person review to add or update a `third_party/` binary; pin and hash everything | `ci/binary-provenance` (fails on unpinned/unhashed/unlicensed blob); mandatory PR review for `third_party/` changes | Legal/Release — `third_party/` |

### 22.7.4 Performance

386SX is the flagship target and the 16-bit external bus is a hard limit. We
manage expectations honestly rather than pretend the hardware is faster.

| Risk ID | Risk | Category | Severity | Probability | Impact | Mitigation | Detection method | Owner / Component |
|---------|------|----------|----------|-------------|--------|------------|------------------|-------------------|
| PERF-01 | 386SX is too slow for some supported titles | Performance | Med | High | Games run sluggishly or below playable framerate on 16-bit-bus machines | Publish honest per-title CPU minimums in `GAMES.INI`; default to the XMS profile for maximum conventional memory and no page-frame overhead; document realistic expectations in `help/`; do not silently ship titles that only run on 486+ as "386SX supported" | `HW-LAB-386SX` timing runs on the reference rig; `compat-report` "slow/unplayable" flag aggregated per title | Perf/Kernel — profiles & tuning |

### 22.7.5 Storage, Filesystem & Data

The installer touches user disks; this is where an error is unrecoverable.
Everything destructive is opt-in, backed up, and verified.

| Risk ID | Risk | Category | Severity | Probability | Impact | Mitigation | Detection method | Owner / Component |
|---------|------|----------|----------|-------------|--------|------------|------------------|-------------------|
| FS-01 | FAT16 size limits and CompactFlash CHS/LBA geometry mismatches | Filesystem & Data | High | Med | Wrong geometry produces an unbootable or truncated volume | `SETUP.EXE` reads BIOS/CF geometry, caps FAT16 partitions at 2 GB, detects CHS-vs-LBA translation mismatches and warns; recommends safe partition sizes for common CF cards | `INST-CHK-1x` geometry guard; `TC-FS-1xx` across CF sizes/geometries; `SETUP.LOG` | Installer — `SETUP.EXE` |
| FS-02 | Data loss installing over an existing DOS system | Filesystem & Data | Critical | Med | User's existing files and configuration destroyed | Non-destructive install is the default; always back up existing `CONFIG.SYS`/`AUTOEXEC.BAT` to `C:\CASTALIA\BACKUP\`; any format or partition write requires an explicit typed confirmation and is logged; offer a dry-run mode | `INST-CHK-2x` destructive-op gate (no write without confirm); `TC-BOOT-2xx` install-over-existing scenarios; `SETUP.LOG` audit | Installer — `SETUP.EXE` |
| FS-03 | Boot-sector installation leaves the machine unbootable | Filesystem & Data | High | Med | Post-install the system won't boot; user stranded | Write then read-back-verify the boot sector; keep a pre-write image of the sector; provide `SAFEBOOT.EXE` and a bootable rescue floppy path; never overwrite an existing boot sector without a saved copy | `INST-CHK-3x` verify-after-write; `TC-BOOT-1xx`; `HW-LAB` cold-boot test after install | Installer — `SETUP.EXE` / boot code |
| FS-04 | SMARTDRV write-behind cache loses data on unclean shutdown | Filesystem & Data | High | Med | Save-games/config corrupted when a retro machine is reset or switched off | Default to read-only caching (write-behind OFF) outside the WIN3X profile; flush the cache on returning to the CASTALIA menu and before launching; document safe power-off; expose the setting in config | `TC-FS-4xx` simulated power-cut under 86Box; `bug` reports tagged `corruption` | Perf/Cache — `SMARTDRV` config |

### 22.7.6 CD-ROM

The CDROM profile isolates the ATAPI stack so its complexity can't destabilize
non-CD sessions.

| Risk ID | Risk | Category | Severity | Probability | Impact | Mitigation | Detection method | Owner / Component |
|---------|------|----------|----------|-------------|--------|------------|------------------|-------------------|
| CD-01 | UIDE/SHSUCDX quirks and drive-letter clashes | CD-ROM | Med | High | CD games can't find the disc; the CD letter collides with a hard-disk letter | Assign the CD letter deterministically with `SHSUCDX /L:`; confine the CD stack (`UIDE.SYS` + `SHSUCDX` loaded high) to the CDROM profile; document the expected letter order; detect and warn on a clash at install | `TC-CD-1xx` (detection, letter, read); `INST-CHK-4x` drive-letter conflict check; `compat-report` tagged `cdrom` | CD subsystem — `UIDE.SYS` / `SHSUCDX` |

### 22.7.7 Sound & Audio

Auto-detection is best-effort; the user can always confirm or override. Resource
conflicts are surfaced before they cause a lockup.

| Risk ID | Risk | Category | Severity | Probability | Impact | Mitigation | Detection method | Owner / Component |
|---------|------|----------|----------|-------------|--------|------------|------------------|-------------------|
| SND-01 | Sound Blaster auto-detection is unreliable | Sound & Audio | Med | High | Wrong or absent audio; games report "no sound card" | `SETSOUND.EXE` probes common ports/IRQ/DMA then asks the user to confirm; writes an explicit `SET BLASTER=A220 I5 D1 H5 T4`-style line; manual selection from the profile list (None/PC Speaker/AdLib/SB 1.5/2.0/Pro/16) always available | `TC-SND-1xx` probe matrix across card models; `HWINFO.EXE` audio probe; `bug` tagged `audio` | Audio — `SETSOUND.EXE` |
| SND-02 | IRQ/DMA conflicts between sound, CD, NIC, and LPT | Sound & Audio | High | Med | System lockups, silent audio, or corrupted DMA transfers | `HWINFO.EXE` builds a resource map and flags overlaps; `SETSOUND.EXE` validates the chosen IRQ/DMA against known-used resources; ship documented safe defaults (A220 I5 D1 H5) and warn before writing a conflicting config | `TC-SND-2xx` conflict scenarios; `HWINFO.EXE` conflict warning in `HWINFO.LOG`; `HW-LAB` on multi-card rigs | Audio — `SETSOUND.EXE` / `HWINFO.EXE` |

### 22.7.8 Input Devices

Low severity, but common enough to matter for first-boot impressions.

| Risk ID | Risk | Category | Severity | Probability | Impact | Mitigation | Detection method | Owner / Component |
|---------|------|----------|----------|-------------|--------|------------|------------------|-------------------|
| INP-01 | CTMOUSE detection / COM-port conflict | Input Devices | Low | Med | No mouse, or a serial mouse contends with a COM port also wanted by a modem | `CTMOUSE` auto-detects serial and PS/2; expose a manual `COMn` override; document serial-vs-PS/2 selection; skip cleanly when no mouse is present | `TC-INP-1xx`; `HWINFO.EXE` mouse/COM probe; `bug` tagged `mouse` | Input — `CTMOUSE` config |
| INP-02 | KEYB layout / codepage mismatch | Input Devices | Low | Med | Wrong keys (ES vs US), missing accented characters | Offer the `KEYB` layout at install with a sane default; document how to change it later; keep the layout selection in one config location | `TC-INP-2xx`; `INST-CHK-5x` layout prompt present; `bug` tagged `keyboard` | Input — `KEYB` config |

### 22.7.9 Windows 3.x

A supported-but-secondary mode. We are explicit about its limits rather than
promising full Windows parity.

| Risk ID | Risk | Category | Severity | Probability | Impact | Mitigation | Detection method | Owner / Component |
|---------|------|----------|----------|-------------|--------|------------|------------------|-------------------|
| WIN-01 | Windows 3.x / WfW 3.11 compatibility limitations | Windows 3.x | Med | Med | Windows is unstable, or 386-enhanced mode fails on some configs | Provide the dedicated `WIN3X` profile (HIMEMX + JEMM386 with EMS + `DOS=HIGH,UMB` + mouse + SMARTDRV); document supported modes and known limits (standard vs 386-enhanced); treat Windows as a secondary target behind DOS gaming | `TC-WIN-1xx`; `HW-LAB` Windows 3.11 boot-and-run; `bug` tagged `win3x` | Windows integration — `WIN3X` profile |

### 22.7.10 User Experience

The profile system is powerful but only helps if users choose correctly.

| Risk ID | Risk | Category | Severity | Probability | Impact | Mitigation | Detection method | Owner / Component |
|---------|------|----------|----------|-------------|--------|------------|------------------|-------------------|
| UX-01 | Users are confused by the boot profiles | User Experience | Med | High | A user runs a game under the wrong profile and concludes it "doesn't work" | Clear, plain boot-menu labels (Maximum Compatibility, XMS Gaming, etc.); `MEMPROF.EXE` recommends the profile per title from `GAMES.INI`; in-menu help pages; XMS as a sensible default for most 90s titles | `TC-CMP-1xx` run under the default profile; `compat-report` correlated to "wrong profile"; periodic docs/UX review | UX/Docs — `CASTALIA.EXE` / `help/` |
| UX-02 | Mixed-language UI feels unfinished | User Experience | Low | Med | A half-Spanish/half-English interface reads as unpolished | Freeze the 1.0 UI to a single English string table; keep all strings in one source of truth; schedule Spanish/Valencian localization as a post-1.0 effort with full-coverage gating | `ci/string-lint` (flags untranslated/duplicate/orphan keys); docs/UX review | UX/Docs — `CASTALIA.EXE` strings |

### 22.7.11 Hardware & Platform

Real hardware is diverse and quirky; conservative defaults plus honest probing
beat clever assumptions.

| Risk ID | Risk | Category | Severity | Probability | Impact | Mitigation | Detection method | Owner / Component |
|---------|------|----------|----------|-------------|--------|------------|------------------|-------------------|
| HW-01 | Real-hardware variance (BIOS quirks, chipset UMB regions, shadow RAM) | Hardware & Platform | High | High | A configuration that passes in emulation fails on a specific board | Choose conservative defaults and probe rather than assume; maintain a tested-hardware matrix; always keep SAFE and CLEAN as guaranteed fallbacks; let `HWINFO.EXE` map chipset/UMB/shadow regions so field reports are actionable | `HW-LAB-*` matrix across reference machines; `HWINFO.EXE` chipset/UMB map; `bug` reports attaching `HWINFO.LOG` | Hardware lab — `HWINFO.EXE` |

### 22.7.12 Testing & QA

Emulators are our fast feedback loop; real hardware is the source of truth.
Neither alone is sufficient.

| Risk ID | Risk | Category | Severity | Probability | Impact | Mitigation | Detection method | Owner / Component |
|---------|------|----------|----------|-------------|--------|------------|------------------|-------------------|
| QA-01 | No automated testing on real hardware | Testing & QA | High | High | Regressions reach real 386SX users undetected | Run a scheduled `HW-LAB` pass on a fixed set of reference machines every milestone; make the hardware checklist a release gate; run a community field-testing program for coverage breadth | `HW-LAB-*` logbook; milestone release gate; aggregated `field` reports | QA — `tests/` + lab |
| QA-02 | Emulator-only validation gives false confidence | Testing & QA | High | Med | Suites are green in DOSBox-X/86Box yet the build is broken on metal | Two-tier testing: fast `TC-EMU-*` smoke gate on every change, plus a mandatory `HW-LAB` sign-off before any tag; document known emulator-vs-metal behavioral deltas so emulator passes are interpreted correctly | `ci/emu-smoke` on PRs; `HW-LAB` sign-off required to cut a tag | QA — emulator + lab |

### 22.7.13 Distribution & Build

Getting a correct image onto period media, reproducibly, is its own discipline.

| Risk ID | Risk | Category | Severity | Probability | Impact | Mitigation | Detection method | Owner / Component |
|---------|------|----------|----------|-------------|--------|------------|------------------|-------------------|
| DIST-01 | Floppy distribution complexity (bad sectors, 720K vs 1.44M, imaging tools) | Distribution & Build | Med | High | Corrupt or unreadable install media; failed installs from disk | Ship separately verified 720K and 1.44M images; CRC every file in the set; document `RAWRITE`/`dd` usage; use a multi-disk set with per-disk verification and a re-image-this-disk recovery path | `ci/floppy-verify` (geometry + per-file CRC on generated images); `TC-DIST-1xx` write-and-read-back; `bug` tagged `media` | Release eng — `floppy/` / `dist/` |
| DIST-02 | Build reproducibility / Open Watcom toolchain drift | Distribution & Build | Low | Med | Two builds from the same source differ; support becomes guesswork | Pin the Open Watcom version; build with `wmake` from a clean tree in CI; record the exact toolchain, flags (`wcl -0 -bt=dos -ml -os`), and artifact hashes in the release notes | `ci/build-watcom` with pinned toolchain; artifact-hash comparison between CI and release | Release eng — `build/` / `scripts/` |

---

## 22.8 Risk-management process

1. **Cadence.** The register is reviewed at every milestone (0.1 Almenara,
   0.2 Peñíscola, 0.5 Morella, 1.0 Tombatossals, 1.1 Montornés) and whenever a
   Critical or High risk materializes in the field.
2. **New risks.** Anyone may propose a risk via a PR to this file or an issue
   labelled `risk`. Each new entry needs a category, severity, probability,
   mitigation, detection method, and an owner before it merges.
3. **Ownership.** Every risk has a single accountable component/team (the last
   column). That owner is responsible for the mitigation and for wiring up the
   named detection method (test case, CI gate, install check, or report channel).
4. **Closure.** A risk is closed only when its mitigation is implemented *and*
   its detection method is live and green. Accepted-but-not-eliminated risks
   (e.g. PERF-01, inherent to the 386SX) stay open with an explicit acceptance
   note in their mitigation and are re-confirmed each milestone.
5. **Release gating.** No release is tagged while any Critical risk lacks an
   implemented mitigation and a passing detection gate. For 1.0 specifically,
   the five risks in §22.6 each have an explicit gate that must be green.

### 22.8.1 Accepted (residual) risks

Some risks cannot be eliminated without abandoning a supported target or the
"compatibility beats elegance" rule. These are accepted for 1.0 with the stated
residual and are re-confirmed each milestone rather than closed.

| Risk ID | Why it cannot be fully eliminated | Residual after mitigation | Re-confirm at |
|---------|-----------------------------------|---------------------------|---------------|
| PERF-01 | The 386SX 16-bit external bus is a hardware fact; we will not drop the flagship target. | Some titles remain slow-but-playable or 486+-only; documented per title. | Every milestone |
| CMP-01 | The universe of DOS titles is effectively unbounded; perfect coverage is impossible. | A long tail of untested/BROKEN titles persists; tracked in `GAMES.INI`. | Every RC |
| HW-01 | We cannot own or emulate every period board, BIOS, and chipset revision. | Rare boards may still need SAFE/CLEAN fallbacks; captured via field reports. | Every milestone |
| WIN-01 | Full Windows 3.x parity is out of scope; DOS gaming is the priority. | 386-enhanced-mode edge cases remain unsupported. | 1.0, 1.1 |

### 22.8.2 Per-milestone risk focus

| Milestone | Primary risk focus |
|-----------|--------------------|
| 0.1 Almenara | FS-02, FS-03, LIC-01 — get install safe and legally clean first. |
| 0.2 Peñíscola | MEM-01, MEM-02, CD-01 — profiles and drivers solid. |
| 0.5 Morella | CMP-01, SND-01, UX-01 — real game coverage and clarity. |
| 1.0 Tombatossals | Top-5 (§22.6) all gated green, HW-LAB sign-off complete. |
| 1.1 Montornés | UX-02, WIN-01, plus `CASTFM.EXE` file-manager risks (new). |

---

## 22.9 Change log

| Date       | Version target | Change                                        |
|------------|----------------|-----------------------------------------------|
| 2026-07-09 | 1.0 Tombatossals | Initial risk register: 27 risks across 13 categories, heat summary, and top-5 1.0 watchlist. |
