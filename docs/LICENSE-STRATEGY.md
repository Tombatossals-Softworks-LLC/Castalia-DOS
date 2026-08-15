# CASTALIA DOS — Section 2: Legal and Licensing Strategy

*Part of the CASTALIA DOS technical bible. Applies to CASTALIA DOS 386SX Edition
and all editions built from this repository.*

Copyright and marks: © 2026 The Castalia DOS Project (repository owner
`davabe`). Original Castalia source code is licensed **MIT** (see the root
`LICENSE` file, which is the controlling text). This document is licensed
**CC BY 4.0**.

> **Status of this document.** This is an engineering compliance plan written by
> the project for the project. It states the rules the project imposes on
> itself so that every published image is lawfully redistributable worldwide. It
> is careful and conservative, but it is **not legal advice**. Where a real
> dispute or an unusual jurisdiction is involved, consult a qualified lawyer.
> When in doubt, the project takes the **stricter** of two readings.

---

## 2.0 Executive summary

CASTALIA DOS is a DOS-compatible operating environment assembled from **FreeDOS**
and other open/free DOS components, wrapped in **original Castalia tools,
configuration, installer, branding, and game profiles**. The legal design has
five load-bearing rules:

1. **Zero Microsoft material.** No MS-DOS files, binaries, message text, manuals,
   fonts, or branding are ever copied, shipped, or cloned byte-for-byte. Not one
   sector of MS-DOS 6.22 touches this project.
2. **FreeDOS is used under its own licenses**, primarily **GPLv2-or-later**. We
   honor those licenses in full, including source availability.
3. **Copyleft is contained.** GPL components live in `third_party/`, ship as
   separate executables and drivers, and are **never statically linked into**
   Castalia's MIT code. The relationship is *aggregation*, not *derivative work*.
4. **Everything is inventoried.** Every third-party component has a recorded
   license, an SPDX identifier taken from its own bundled text, a source
   location, and a stated obligation in a machine-checkable manifest.
5. **Source ships with the media.** Where any license (GPL, LGPL) requires source
   availability, CASTALIA DOS ships the **complete corresponding source on the
   same distribution media / in the same release**, not by written offer.

If any one of these rules cannot be satisfied for a component, that component
does not ship. There are no exceptions and no "we'll fix it later" releases.

---

## 2.1 Why we cannot include or clone MS-DOS 6.22

MS-DOS is not free software and never was. Its files are protected by two
independent legal regimes that must **both** be respected: **copyright** (protects
the code, text, and creative expression) and **trademark** (protects the names
and marks that identify the product's source). They are separate; clearing one
does not clear the other.

### 2.1.1 Copyright

| MS-DOS asset | Why it is off-limits |
|---|---|
| `IO.SYS`, `MSDOS.SYS`, `COMMAND.COM` | Microsoft-authored object code. Copying, decompiling into shipped code, or redistributing is infringement. |
| External commands (`FORMAT`, `FDISK`, `XCOPY`, `SMARTDRV.EXE`, `EMM386.EXE`, `HIMEM.SYS`, `MSCDEX.EXE`, `MSD.EXE`, etc.) | Each is a separate copyrighted Microsoft program. |
| On-screen message text and prompts | Human-readable strings are creative expression. We do not lift Microsoft's wording. |
| Manuals, help files, `HELP.HLP`, README text | Documentation is fully copyrighted; verbatim reuse or paraphrase-to-the-bone is infringement. |
| Fonts, logos, the startup banner art | Creative works, protected independently of the code. |

The 2018 open-sourcing of **MS-DOS 1.25 and 2.0** by Microsoft (MIT-licensed, on
GitHub) changes nothing for us: it covers only those two ancient versions, and we
target **6.22-era behavior**. We do not copy even the MIT-released 1.x/2.0 code,
because it is 8086-era and irrelevant, and mixing it in would muddy provenance.

**What copyright does *not* protect** is the important escape hatch: it does not
protect **facts, methods of operation, interfaces, or APIs**. The DOS system-call
interface (INT 21h function numbers, register conventions, FAT12/FAT16 on-disk
structures, the MZ `.EXE` header, the PSP layout, the `SET BLASTER` environment
convention) is *functional*, not expressive. We are free to implement those
interfaces from published specifications. FreeDOS already did exactly this, which
is why FreeDOS is lawful — and why we build on FreeDOS instead of on MS-DOS.

### 2.1.2 Trademark

Copyright asks "did you copy the work?" Trademark asks "are you confusing people
about who made or endorsed this?"

| Mark (Microsoft-owned) | Our rule |
|---|---|
| **MS-DOS**, **Microsoft**, **Windows**, **SmartDrive** | Never used *as* our product name, logo, file identity, or banner. Never used to imply Microsoft made or endorsed CASTALIA DOS. |
| Same marks, used descriptively | **Nominative fair use** is allowed: "compatible with games written for MS-DOS," "runs Windows 3.x." We use only as much of the mark as needed, add no logos, and never suggest sponsorship. |
| Version numbers ("6.22", "7.x") | A bare number is not a trademark. Reporting DOS version `6.22` via INT 21h/AH=30h or via `SETVER` is a **functional** value, not branding. |

Concrete consequences for the product:

- The boot banner says **"CASTALIA DOS"**, never "MS-DOS". Where we describe
  compatibility we write "MS-DOS 6.22-compatible" — a descriptive claim, with
  "compatible" always present, never "MS-DOS 6.22" standing alone as if it were
  our own product name.
- The **"Windows 3.x Mode"** boot-menu label (profile `WIN3X`) is a descriptive,
  nominative reference to the environment it prepares the machine for. It does
  not bundle, clone, or ship any Microsoft Windows file.
- The disk-cache profile keyword is historically `SMARTDRV`. "SmartDrive" is a
  Microsoft product name. We ship a **free cache under its own identity** (see
  §2.5) and only expose a compatibility command name where legacy batch files
  expect it; we never claim the free tool *is* Microsoft SmartDrive, and the
  THIRD-PARTY manifest records its real name and author. Where feasible we prefer
  a neutral command alias over reusing the `SMARTDRV` name outright.

### 2.1.3 Clean-room discipline

Because we assemble existing free implementations, we rarely need to reimplement
anything from scratch. But when we *do* write original code that reproduces a
DOS behavior, we follow clean-room principles so the result is provably
independent:

1. **Specify from public interface docs, not from Microsoft source.** Ralf
   Brown's Interrupt List (RBIL), published DOS programmer references, the
   FreeDOS sources, and the FAT specification describe *interfaces*. Interfaces
   are lawful to implement. We never disassemble a Microsoft binary and transcribe
   its instructions into shipped code.
2. **Separate "what" from "how".** One person/document may describe the required
   externally-observable behavior (the "what"); the implementer writes the "how"
   without ever seeing Microsoft's implementation.
3. **Author our own message text.** We write our own wording for prompts and
   errors. We match Microsoft phrasing only where a string is a de-facto
   *functional* token that other software parses programmatically (rare in DOS);
   even then we prefer to match the documented behavior, not lift the sentence.
4. **Record provenance.** Every original tool carries a header noting it is an
   independent implementation from public specifications, with no Microsoft code.

---

## 2.2 Using FreeDOS legally

FreeDOS is a complete, actively maintained, free DOS. It is not one license but a
*collection*; the core is copyleft, and some utilities are more permissive. The
two pieces we depend on most are:

| FreeDOS piece | What it is | License |
|---|---|---|
| **FreeDOS kernel** (`KERNEL.SYS`) | The DOS-C-lineage kernel (Pat Villani et al.) | **GPL-2.0-or-later** |
| **FreeCOM** (`COMMAND.COM`) | The FreeDOS command interpreter/shell | **GPL-2.0-or-later** |

### 2.2.1 What the FreeDOS licenses grant us

The GPL is a *grant*, not merely a restriction. Under GPLv2+ we already have the
right to do everything CASTALIA DOS needs:

| Right we exercise | Granted by | Condition we must meet |
|---|---|---|
| Redistribute the kernel and shell, unmodified or modified | GPLv2 §1–2 | Keep license + copyright notices; provide source (§2.3). |
| Modify the kernel/shell (patches, config, defaults) | GPLv2 §2 | Mark changes, keep it GPL, provide the modified source. |
| **Sell** media/downloads that include FreeDOS | GPLv2 §1 | May charge for the copy/medium; source still available at no more than cost. |
| Bundle FreeDOS with our own separate programs | GPLv2 §2 (final ¶) | "Mere aggregation" — see §2.4. |

### 2.2.2 The FreeDOS "OEM" / distribution allowance, correctly stated

There is a persistent myth of a special "FreeDOS OEM license." **There is no
separate OEM contract or EULA.** FreeDOS carries no click-through agreement. The
FreeDOS Project (Jim Hall and contributors) *explicitly encourages* redistribution,
rebranding into derived distributions, embedding in products, and even commercial
sale — because the underlying licenses (mostly GPL, with some BSD/public-domain
utilities) already permit it. "OEM distribution" simply means: **exercise the GPL
rights and honor the GPL obligations.** For CASTALIA DOS that means source
availability, notice retention, and honest attribution.

Two courtesy (not legal) practices we still follow, because they are correct and
cheap:

- **Attribute FreeDOS by name.** "CASTALIA DOS is based on FreeDOS." This is
  honest, it is good community citizenship, and it partly overlaps the GPL's
  notice-retention duty anyway.
- **Do not imply endorsement.** "Based on FreeDOS" is a statement of fact.
  We do not present CASTALIA DOS as an *official* FreeDOS product or use the
  FreeDOS logo in a way that suggests the FreeDOS Project sponsors us.

### 2.2.3 Versioning honesty

The FreeDOS kernel reports DOS version **7.x**. CASTALIA DOS presents itself as
**"6.22-compatible"** and uses `SETVER` *per program* for software that demands a
specific version. We deliberately do **not** claim a global 6.22 spoof, and we
never ship a banner reading "MS-DOS 6.22". Reporting a version number is
functional; wearing Microsoft's name is not permitted.

---

## 2.3 GPL implications when we modify the FreeDOS kernel

We *do* modify FreeDOS components (kernel build options, FreeCOM defaults,
possibly small patches). This section states exactly what that triggers.

### 2.3.1 What "modify" triggers, and what it does not

| Action on a GPL component | Triggers copyleft source duty? | Notes |
|---|---|---|
| Ship the kernel binary **unmodified** | **Yes** — GPLv2 §3 still requires source availability | Even verbatim redistribution requires offering the corresponding source. |
| Recompile the kernel with different build flags | **Yes** | The build configuration is part of the corresponding source; record it. |
| Patch kernel C/ASM source | **Yes** | Patched source must ship; mark the changed files and dates. |
| Change `CONFIG.SYS` / `AUTOEXEC.BAT` / `.INI` that the kernel *reads* | **No** | Config data is not a modification of the program's source. |
| Bundle the kernel next to our MIT `.EXE`s on one disk | **No** (aggregation) | See §2.4. |
| Call the kernel's INT 21h services from our MIT tool | **No** | Using a documented syscall interface is not linking; it does not make our tool a derivative. |

### 2.3.2 The three obligations copyleft imposes on us

When we ship a modified (or even unmodified) GPL binary, GPLv2 requires:

1. **Source availability (§3).** The *complete corresponding source code* — the
   source we actually built from, plus the scripts/makefiles/build config needed
   to build and install it. CASTALIA DOS satisfies this **with the media** (§2.7).
2. **License and notice retention (§1).** Ship the GPL text; keep all copyright
   and license notices intact; do not add further restrictions.
3. **Change marking (§2a).** Modified files carry prominent notices stating that
   we changed them and the date of change.

### 2.3.2a Recorded modifications — the boot sign-on

CASTALIA DOS modifies two FreeDOS components at build time — FreeCOM's banner,
and the kernel (rebuilt from source as the *Castalia kernel*). All changes are
marked here per GPLv2 §2a. The kernel changes go beyond cosmetics: the rebuilt
kernel carries a Castalia OEM identity, a boot-profile directive, and a Castalia
identity syscall, so it is a genuine derivative work — not a renamed binary.

**(1) The FreeCOM banner is blanked.**
[`scripts/rebrand-dos.py`](../scripts/rebrand-dos.py) blanks FreeCOM's
"FreeCom version 0.85a - WATCOMC - XMS_Swap [date]" line in the freshly
fetched `COMMAND.COM` — one printf format string, in place, length-preserving,
newlines kept, no shell logic touched. It deliberately does **not** touch
FreeCOM's internal "FreeDOS STRINGS" resource magic, its version-detection
diagnostics, or the dormant Open Watcom C run-time copyright (never printed;
present in every Open-Watcom-built binary, ours included).

**(2) The kernel is rebuilt from source as the *Castalia kernel*.** The stock
FreeDOS 1.3 kernel prints "FreeDOS kernel … - WATCOMC …" and the Villani/GPL
copyright *unconditionally*, from a packed (exeflat + UPX) image whose strings
cannot be byte-patched. So [`scripts/build-kernel.sh`](../scripts/build-kernel.sh)
rebuilds the kernel from its official, pinned source (`ke2043s.zip`) and
[`scripts/patch-kernel-src.py`](../scripts/patch-kernel-src.py) applies a small,
anchored, idempotent set of marked source changes, then ships it uncompressed:

- **`signon()`** (`kernel/main.c`) is rewritten to a single `CASTALIA DOS 386SX
  Edition` line, keeping the original arguments so no symbol is orphaned.
- **The OEM identity** is changed from `0xFD` (FreeDOS) to `0xCA` (Castalia) in
  `hdr/version.h` and the resident VERSION resource in `kernel/kernel.asm`, so
  `INT 21h`/`AH=30h` reports Castalia in `BH`.
- **A resident `castalia_boot_profile` byte** (`kernel/globals.h`) and a
  **`CASTALIA=` `CONFIG.SYS` directive** (`kernel/config.c`) record which of the
  eight boot profiles is active.
- **An `INT 2Fh`/`AH=0CAh` identity multiplex** (`kernel/int2f.asm`) reports the
  Castalia signature, build number, edition, OEM id, and active boot profile to
  userland.

These are the only changes; everything else is stock FreeDOS 1.3 build 2043. The
register-level contract is documented in [`docs/KERNEL.md`](KERNEL.md) and is
consumed by `HWINFO.EXE` and the `CASTALIA.EXE` About screen.

We satisfy the three obligations for all of these edits as follows:

1. **Source availability.** The transformations are fully described by
   `scripts/rebrand-dos.py`, `scripts/patch-kernel-src.py`, and
   `scripts/build-kernel.sh`, which ship in the repo and on the media. For the
   kernel, the *corresponding source* is the pinned upstream `ke2043s.zip` plus
   that patch script; a release additionally carries the FreeDOS source in
   `third_party/`.
2. **Notice retention.** FreeDOS's copyright, the GPL text, and the warranty
   disclaimer are retained in the shipped source (`third_party/`), in
   `LICENSES/`, and in the Castalia **About** screen ("Built on FreeDOS
   (GPLv2+) … source in third_party\\"). We remove only the runtime *sign-on
   banners*, never a licence or copyright notice, and never from the
   distribution.
3. **Change marking.** This subsection, the scripts' own headers, and the
   commit history state plainly what we modify and when. FreeDOS explicitly
   encourages rebranding into derived distributions (§2.2.2); we keep the
   attribution honest and imply no endorsement.

### 2.3.3 Scope of copyleft — how far does the "virus" reach?

This is the question that decides the whole architecture. GPL's copyleft attaches
to a **work based on** the GPL program — a *derivative work*. It does **not**
reach every file that happens to sit on the same disk.

- **Inside the copyleft boundary:** the kernel source and our patches to it;
  FreeCOM source and our patches to it; anything we create by copying GPL code
  into it or by statically linking GPL object code into it.
- **Outside the copyleft boundary:** our independent MIT programs that merely run
  *on top of* the DOS, communicate with it only through the documented syscall
  interface (INT 21h etc.), and are shipped as **separate executables**. These
  are aggregated with — not derived from — the GPL kernel.

The dividing line is **linkage and derivation**, not co-residence. §2.4 makes the
architecture that keeps our code outside the boundary explicit and enforceable.

---

## 2.4 Keeping Castalia tools separate from GPL components

The commercial and legal freedom of CASTALIA DOS depends on our original tools
staying **MIT** and *not* being pulled into GPL. We achieve that structurally, not
by hoping.

### 2.4.1 Derivative work vs. mere aggregation

GPLv2 §2, final paragraph — the **"mere aggregation" clause** — is the text we
rely on:

> "…mere aggregation of another work not based on the Program with the Program
> (or with a work based on the Program) on a volume of a storage or distribution
> medium does not bring the other work under the scope of this License."

Putting our MIT `CASTALIA.EXE`, `LAUNCH.EXE`, `MEMPROF.EXE`, etc. **on the same
diskette / CF card / disk image** as the GPL `KERNEL.SYS` and `COMMAND.COM` is
exactly this "mere aggregation on a volume." It does **not** relicense our tools.

| | Derivative work (copyleft spreads) | Mere aggregation (copyleft contained) |
|---|---|---|
| Static-link GPL object code into our `.EXE` | Yes — becomes GPL | — |
| Copy GPL source lines into our tool | Yes — becomes GPL | — |
| Ship our separate `.EXE` beside the kernel on one image | — | **Yes — stays MIT** |
| Our tool calls INT 21h / a DOS device driver | — | **Yes — stays MIT** |
| Our tool `EXEC`s (spawns) a GPL program as a child process | — | **Yes — stays MIT** |
| Our tool reads/writes files a GPL tool also uses | — | **Yes — stays MIT** |

### 2.4.2 The hard rules for Castalia code

1. **No static linking of GPL into MIT code.** Castalia tools never `#include`
   GPL headers that carry code, never link GPL `.obj`/`.lib`, and never embed GPL
   source. In 16-bit DOS there are no shared libraries, so "linking" means static
   linking — which we simply do not do across the MIT/GPL boundary.
2. **Interfaces only.** Castalia tools talk to FreeDOS through documented
   interfaces: INT 21h and other DOS/BIOS interrupts, device drivers loaded via
   `CONFIG.SYS`, environment variables (`BLASTER`, `PATH`), and files
   (`CONFIG.SYS`, `AUTOEXEC.BAT`, `*.INI`). Interface use is not derivation.
3. **Separate executables and drivers.** Every Castalia component is its own
   `.EXE`/`.COM`/`.SYS`. We do not merge Castalia logic into the kernel or shell.
4. **Cooperate by process and file, not by link.** When a Castalia tool needs a
   GPL utility's behavior, it *spawns* that utility as a child process or writes a
   config the GPL tool later reads — arms-length cooperation, never in-address-space
   linkage of copied code.
5. **Repository segregation mirrors the legal boundary** (§2.4.3).

### 2.4.3 Repository layout enforces the boundary

```
Castalia-DOS/
  LICENSE                     MIT — controls original Castalia code
  LICENSES/                   full text of every license we ship under
    MIT.txt
    GPL-2.0.txt
    LGPL-2.1.txt
    BSD-2-Clause.txt
    CC-BY-4.0.txt
    CC0-1.0.txt
    LicenseRef-PublicDomain.txt
  src/                        MIT — original Castalia sources (Open Watcom C)
    castalia/  launch/  memprof/  setsound/  hwinfo/  setup/  common/
  third_party/                NON-MIT — vendored upstream, one dir per component
    MANIFEST.yml              machine-readable inventory (see §2.6)
    freedos-kernel/           GPL-2.0-or-later  (+ our patches/ + SOURCE/)
    freecom/                  GPL-2.0-or-later
    himemx/  jemm386/  ctmouse/  uide/  shsucdx/  keyb/  setver/  lbacache/
  docs/                       CC BY 4.0 — this bible
  help/                       CC BY 4.0 — on-disk help pages
  config/                     Castalia config templates (CONFIG.SYS, *.INI)
  scripts/                    MIT — build/install scripts
  build/                      build outputs (not the source of truth)
  floppy/  dist/              staged and finished distribution images
  tests/  tools/
```

Rule of thumb: **if it is not ours, it lives in `third_party/` with its license
and its source.** `src/` contains only MIT-licensed Castalia code. A file's
directory tells you its license family at a glance, and the release gate (§2.10)
fails the build if a GPL source file appears under `src/` or an un-inventoried
component appears under `third_party/`.

---

## 2.5 Component license inventory

This is the authoritative per-component table for the shipped system. **SPDX**
identifiers are used. "Copyleft?" means: does redistribution obligate us to keep
it under the same license and ship source? "Source ship" means: must the
corresponding source accompany the binary?

> **Governing rule for this table.** The SPDX identifier recorded in
> `third_party/MANIFEST.yml` is copied **verbatim from the exact pinned version's
> own bundled license/readme**. Where the table below and a component's bundled
> text disagree for the version we actually ship, **the bundled text wins** and
> the manifest is corrected. Where a component's license is genuinely ambiguous,
> we default to the **stricter** assumption (treat as copyleft, ship source)
> until its own license text proves otherwise. That conservative default can only
> ever *over*-comply, never under-comply.

| Component | File(s) | SPDX license | Copyleft? | Source ship? | Repo location |
|---|---|---|---|---|---|
| FreeDOS kernel | `KERNEL.SYS` | GPL-2.0-or-later | **Yes** | **Yes** | `third_party/freedos-kernel/` |
| FreeCOM shell | `COMMAND.COM` | GPL-2.0-or-later | **Yes** | **Yes** | `third_party/freecom/` |
| HIMEMX (XMS mgr) | `HIMEMX.EXE` | GPL-2.0-or-later † | **Yes** † | **Yes** † | `third_party/himemx/` |
| JEMM386 (EMM386-compat) | `JEMM386.EXE` | GPL-2.0-or-later † (conservative) | **Yes** † | **Yes** † | `third_party/jemm386/` |
| CTMOUSE (CuteMouse) | `CTMOUSE.EXE` | GPL (verified from the package) | **Yes** | **Yes** | `third_party/ctmouse/` |
| UIDE (IDE/ATAPI CD) | `UIDE.SYS` | LicenseRef-PublicDomain † (freeware, src incl.) | No | No (ship anyway) | `third_party/uide/` |
| SHSUCDX (MSCDEX repl.) | `SHSUCDX.COM` | GPL-2.0-or-later † (conservative) | **Yes** † | **Yes** † | `third_party/shsucdx/` |
| KEYB (keyboard) | `KEYB.EXE` + `.KL` data | GPL-2.0-or-later | **Yes** | **Yes** | `third_party/keyb/` |
| SETVER (version spoof) | `SETVER.EXE` | GPL-2.0-or-later | **Yes** | **Yes** | `third_party/setver/` |
| SmartDrive-style cache | `LBACACHE`/cache `.COM` | LGPL-2.1-or-later † | **Yes** (weak) | **Yes** † | `third_party/lbacache/` |
| Castalia original tools | `CASTALIA.EXE`, `LAUNCH.EXE`, `MEMPROF.EXE`, `SETSOUND.EXE`, `HWINFO.EXE`, `CFGEDIT.EXE`, `SAFEBOOT.EXE`, `GAMECFG.EXE` (later `CASTFM.EXE`) | MIT | No | Yes, by choice | `src/` |
| Castalia documentation | `docs/`, `help/` pages | CC-BY-4.0 | No (attribution) | n/a (text) | `docs/`, `help/` |
| Castalia branding/artwork | logos, boot art, marks | Marks proprietary; art CC-BY-4.0 (see §2.9.3) | No | n/a | `dist/brand/` |

† **Must be re-verified against the shipped version's own license file at pin
time** (release gate §2.10, step 3). These entries reflect best current knowledge;
where the component is genuinely ambiguous we assume the copyleft/source-shipping
duty so we cannot under-comply. FreeDOS `EMM386` (GPL-2.0-or-later) is the drop-in
fallback for JEMM386 and carries identical obligations.

### 2.5.1 Notes on individual components

- **FreeDOS kernel & FreeCOM** are the two unavoidable GPL cornerstones. Their
  source and our patches are the main content of the shipped source archive.
- **HIMEMX / JEMM386** (Michael Devore / Japheth lineage) are treated as copyleft
  with source shipped. If a pinned version's own readme proves a more permissive
  grant, the manifest relaxes — but we never *assume* the relaxation.
- **CTMOUSE** is **GPL**, not BSD-2-Clause. Earlier revisions of this document
  recorded it as permissive "per project convention"; the shipped package says
  otherwise — `DOC/CTMOUSE/CTMOUSE.TXT`: "CuteMouse is released under the terms
  of the GNU General Public License (GPL)", © 1997-2002 Nagy Daniel. So its
  source is a **duty**, not a courtesy. The package carries its own
  `SOURCE/CTMOUSE/SOURCES.ZIP`, which `fetch-payload.sh --with-sources` stages.
  This is why the manifest is built by reading packages rather than by
  convention.
- **UIDE.SYS** (Jack R. Ellis lineage) ships as freeware with source included; we
  keep its readme/notice verbatim and ship its source regardless of whether the
  license strictly compels it.
- **SHSUCDX** provides the MSCDEX-replacement so CD games see a CD-ROM redirector.
  Treated as copyleft/source-shipped pending per-version verification.
- **KEYB / SETVER** are FreeDOS GPL utilities; standard GPL obligations.
- **SmartDrive-style cache** is a free (LGPL/GPL-family) disk cache — *not*
  Microsoft SmartDrive. LGPL's "must allow relinking" clause is effectively moot
  in a statically-linked 16-bit DOS world; we discharge it simply by **shipping
  the cache's complete source**, which satisfies the stronger GPL duty too. The
  `SMARTDRV` profile keyword and any `SMARTDRV` command name are compatibility
  shims (§2.1.2); the manifest names the real tool and author.

---

## 2.6 Documenting third-party licenses

Three layers, each with a different audience: (a) full license texts, (b) a
machine-readable manifest, (c) a human-readable NOTICE on the media.

### 2.6.1 `LICENSES/` — the full texts

`LICENSES/` holds the **complete, verbatim** text of every license we distribute
under, one file per SPDX identifier (`GPL-2.0.txt`, `LGPL-2.1.txt`,
`BSD-2-Clause.txt`, `MIT.txt`, `CC-BY-4.0.txt`, `CC0-1.0.txt`,
`LicenseRef-PublicDomain.txt`). Every SPDX id that appears in the manifest **must**
have a matching file here; the release gate enforces this both ways (no orphan
texts, no missing texts).

### 2.6.2 `third_party/MANIFEST.yml` — the machine-readable inventory

One entry per vendored component. Fields:

| Field | Meaning |
|---|---|
| `name` | Human name of the component |
| `files` | Installed artifact filename(s) on the media |
| `version` | Exact upstream version/tag/commit pinned |
| `spdx` | SPDX license identifier, copied from the pinned version's own text |
| `copyleft` | `strong` \| `weak` \| `none` |
| `source_required` | `true` \| `false` — does its license compel source shipping |
| `source` | Path to the corresponding source we ship (`SOURCE/…`) |
| `upstream` | Canonical upstream URL for the pinned version |
| `notice` | Path to the component's own license/readme copy |
| `modified` | `true`/`false` — did we patch it |
| `patches` | Path to our patch set, if `modified` |
| `install_path` | Where it lands on the target (`C:\DOS`, `C:\CASTALIA\DRV`, …) |

Example entry:

```yaml
- name: FreeDOS kernel
  files: [KERNEL.SYS]
  version: "1.3-pre-<pinned-tag>"
  spdx: GPL-2.0-or-later
  copyleft: strong
  source_required: true
  source: third_party/freedos-kernel/SOURCE/
  upstream: https://github.com/FDOS/kernel (tag <pinned>)
  notice: third_party/freedos-kernel/COPYING
  modified: true
  patches: third_party/freedos-kernel/patches/
  install_path: 'C:\'
```

### 2.6.3 `THIRD-PARTY.txt` / `NOTICE` — on the distribution media

Every shipped image carries a plain-text `THIRD-PARTY.txt` (a.k.a. `NOTICE`) at
the root of the media, readable with `TYPE` on the target machine itself. It is
generated from `MANIFEST.yml` so it can never drift. It lists, per component:
name, version, author/upstream, license (with pointer to the full text in
`LICENSES\`), whether we modified it, and where its source is. It closes with the
source-availability statement (§2.7) and the trademark disclaimer (§2.9.4).

```
CASTALIA DOS 386SX Edition — THIRD-PARTY NOTICES
================================================
This product includes free/open-source software. Full license texts are in
the LICENSES\ directory on this medium. Corresponding source for all
copyleft components is provided on this medium under \SOURCE (see below).

FreeDOS kernel (KERNEL.SYS) .... GPL-2.0-or-later .. modified .. \SOURCE\KERNEL
FreeCOM (COMMAND.COM) .......... GPL-2.0-or-later .. as-is ..... \SOURCE\FREECOM
HIMEMX (HIMEMX.EXE) ............ GPL-2.0-or-later .. as-is ..... \SOURCE\HIMEMX
JEMM386 (JEMM386.EXE) .......... GPL-2.0-or-later .. as-is ..... \SOURCE\JEMM386
CTMOUSE (CTMOUSE.EXE) .......... GPL ............... as-is ..... \SOURCES\CTMOUSE
UIDE (UIDE.SYS) ............... Public Domain/free  as-is ..... (source incl.)
SHSUCDX (SHSUCDX.COM) ......... GPL-2.0-or-later .. as-is ..... \SOURCE\SHSUCDX
KEYB (KEYB.EXE) ............... GPL-2.0-or-later .. as-is ..... \SOURCE\KEYB
SETVER (SETVER.EXE) .......... GPL-2.0-or-later .. as-is ..... \SOURCE\SETVER
Disk cache (LBACACHE) ......... LGPL-2.1-or-later . as-is ..... \SOURCE\CACHE

Original CASTALIA DOS tools, installer, and scripts are (C) 2026 The
Castalia DOS Project, licensed MIT (LICENSES\MIT.txt). Documentation is
CC BY 4.0. "MS-DOS", "Microsoft", and "Windows" are trademarks of Microsoft
Corporation; CASTALIA DOS is an independent product, not affiliated with or
endorsed by Microsoft. "Based on FreeDOS."
```

---

## 2.7 Shipping source code when the GPL requires it

GPLv2 §3 gives a distributor of binaries three ways to satisfy the source duty:

| GPLv2 §3 option | What it means | Fit for CASTALIA DOS |
|---|---|---|
| **§3(a) — with the media** | Ship complete corresponding source *on the same medium / in the same release*. | **CHOSEN.** Simplest, self-contained, permanently valid. |
| **§3(b) — written offer** | Include a written offer, valid ≥3 years, to send source to any third party for no more than cost. | Rejected: creates a 3-year fulfillment obligation and record-keeping burden; risky for a small project. |
| **§3(c) — pass along an offer** | Only for noncommercial redistribution of a binary you received with a §3(b) offer. | Not applicable — we are the originating distributor. |

### 2.7.1 CASTALIA DOS chooses §3(a): source with the media

**Rationale:**

1. **Self-contained and durable.** A CF card, ISO, or `.img` that includes its own
   source can be copied, archived, and re-shared by anyone forever, with the
   source riding along automatically. A §3(b) offer, by contrast, can outlive the
   project's ability to honor it.
2. **No dangling obligations.** A written offer must be honorable for at least
   three years to *any* third party. A small retro project should not carry a
   multi-year mail-order-source liability.
3. **Matches how the media is consumed.** We already master full disk images; the
   marginal cost of a `\SOURCE` tree or a `SOURCE.ZIP`/source `.img` is trivial.

**Mechanics:**

- Every release includes the **complete corresponding source** for **every
  copyleft component actually shipped in that release** — the exact upstream
  source at the pinned version **plus our patches** plus the build scripts,
  makefiles, and configuration needed to rebuild the shipped binaries. "As we
  built it," not "latest upstream."
- Delivery form by medium:
  - **ISO / hard-disk / CF images:** a `\SOURCE` directory tree on the image.
  - **Floppy sets** (where space forbids on-disk source): a companion
    **source floppy/ZIP** in the *same* release bundle, plus a durable mirror
    URL printed in `THIRD-PARTY.txt`. The with-media source in the same release
    is what discharges §3(a); the mirror is a convenience, not the legal basis.
- **Permissive components ship source too.** BSD/public-domain/freeware
  components (UIDE, SHSUCDX) do not strictly require it, but shipping their source
  is cheap, aids reproducibility, and honors their notice requirements — so we do
  it uniformly. One rule ("ship all the source") is easier to get right than a
  per-component judgment call under release pressure.
- **MIT Castalia source** is public in the repository; we also include it in
  `\SOURCE` for a fully self-describing medium.

### 2.7.2 "Complete corresponding source" checklist for a copyleft component

- [ ] Exact upstream source at the **pinned** version/tag/commit.
- [ ] Our patches, as separate, readable patch files, with change notices.
- [ ] The **build recipe**: makefiles, `wmake`/`wcl` invocations, flags, and the
      compiler/assembler version used (Open Watcom / target build env).
- [ ] Any config or data compiled into the binary.
- [ ] The component's own license and readme, verbatim.
- [ ] A one-line pointer in `THIRD-PARTY.txt` to its `\SOURCE\<component>` path.

---

## 2.8 Making the whole distribution legally redistributable — checklist

A release is redistributable **only if every box is checked.** This is the gate
between "we built an image" and "we may publish it."

- [ ] **No Microsoft material.** No MS-DOS binaries, message text, manuals, fonts,
      or logos anywhere on the image or in the repo. Provenance of every file is
      known and free.
- [ ] **Every third-party file is in the manifest.** No un-inventoried file exists
      under `third_party/` or on the media. No stray binary of unknown origin.
- [ ] **Every SPDX id has its full text** in `LICENSES/` (and in `LICENSES\` on
      the media); no missing texts, no orphan texts.
- [ ] **Every copyleft binary has its corresponding source** in the release
      (`\SOURCE`), at the pinned version, with patches and build recipe.
- [ ] **All upstream notices retained** — copyright headers, `COPYING`, readmes
      copied intact; nothing stripped.
- [ ] **Modifications marked** — patched GPL files carry "changed by The Castalia
      DOS Project, <date>" notices; patches are shipped.
- [ ] **`THIRD-PARTY.txt` present on the media**, generated from the manifest,
      accurate, with the trademark disclaimer and source statement.
- [ ] **No GPL code linked into MIT binaries.** Castalia `.EXE`s are independent;
      boundary respected (§2.4).
- [ ] **Trademark hygiene.** Product is "CASTALIA DOS"; MS marks used only
      descriptively; disclaimer present; no MS/Windows logos.
- [ ] **License compatibility verified.** No component forbids redistribution or
      commercial use; no "non-commercial", "no-derivatives", or field-of-use
      restriction sneaked in via a utility or artwork asset.
- [ ] **Data assets cleared.** Game *profiles* describe games (facts/config); no
      copyrighted game binaries, box art, or ROMs are shipped (§2.9.5).
- [ ] **Version pins recorded** so the exact shipped bits are reproducible.

---

## 2.9 Recommended license strategy — per asset class

This is the canonical mapping the project commits to. It matches the repo `LICENSE`
and the conventions.

| Asset class | License | SPDX | Rationale |
|---|---|---|---|
| Castalia original tools (code) | MIT | `MIT` | Maximal reuse/embedding; compatible with GPL *aggregation*; matches root `LICENSE`. |
| Castalia documentation | CC BY 4.0 | `CC-BY-4.0` | Right license for prose; requires only attribution; freely mirror-able. |
| Castalia branding / artwork | Marks proprietary; decorative art CC BY 4.0 | mixed | Names/logos are trademarks the project controls; loose decorative art is shareable. |
| Modified FreeDOS components | GPLv2-or-later | `GPL-2.0-or-later` | Non-negotiable: derived from GPL; stays GPL, source shipped. |
| Installer / build scripts | MIT | `MIT` | Ours; permissive; may be copied by downstreams. |
| Game launcher DB / `GAMES.INI` | CC0 (preferred) or CC BY 4.0 | `CC0-1.0` / `CC-BY-4.0` | It is **data**, not program code — see §2.9.5. |

### 2.9.1 Castalia original tools — MIT

`CASTALIA.EXE`, `LAUNCH.EXE`/`GAMEVAULT.EXE`, `MEMPROF.EXE`, `SETSOUND.EXE`,
`HWINFO.EXE`, `CFGEDIT.EXE`, `SAFEBOOT.EXE`, `GAMECFG.EXE`, and the planned
`CASTFM.EXE`/`CFM.EXE`/`ALCAZAR.EXE`. MIT keeps them permissively reusable and
keeps them **out** of copyleft: because they are separate executables that only
speak to FreeDOS through interfaces (§2.4), aggregating them with the GPL kernel
does not relicense them. Every source file carries the MIT header and the
"independent implementation, no Microsoft code" provenance note.

### 2.9.2 Castalia documentation — CC BY 4.0

This bible, on-disk `HELP` pages, and website docs. CC BY 4.0 fits prose (not
code), permits mirroring and translation, and asks only for attribution to
The Castalia DOS Project.

### 2.9.3 Castalia branding / artwork — split the layer

- **Trademarks** ("CASTALIA DOS", the "386SX Edition" wordmark, the fortress
  logo/boot emblem): controlled by the project as **marks**, *not* placed under a
  free license. Others may make CASTALIA DOS available, but may not ship a
  modified build *under the CASTALIA DOS name/logo* in a way that implies it is
  the official product. This is standard "free code, protected mark" practice
  (as Firefox/Iceweasel demonstrated) and is fully compatible with GPL/MIT, which
  govern *code*, not *marks*.
- **Decorative artwork** (palette swatches, non-identifying UI ornaments, sample
  wallpapers): **CC BY 4.0**, so the community can reuse the aesthetic without
  touching the marks.
- A short `BRANDING.md` (future) will state the permitted/forbidden uses of the
  name and logo precisely.

### 2.9.4 Trademark disclaimer (ships in `THIRD-PARTY.txt` and docs)

> "MS-DOS", "Microsoft", and "Windows" are trademarks of Microsoft Corporation.
> "CASTALIA DOS" is an independent, unaffiliated product and is not sponsored or
> endorsed by Microsoft. "CASTALIA DOS" and its logo are marks of The Castalia
> DOS Project. Based on FreeDOS.

### 2.9.5 Game launcher database / `GAMES.INI` — CC0 (preferred), CC BY 4.0 (alternative)

`GAMES.INI` and the launcher profiles are **data describing games**: the title we
show, the executable name to run, the recommended memory profile (`CLEAN`/`XMS`/
`EMS`/`CDROM`/`WIN3X`), sound settings, and notes. This is *facts + configuration*,
not program code — so a code license (MIT/GPL) is the wrong tool.

- **CC0 (public-domain dedication) — recommended.** Most fields are
  uncopyrightable facts (a game's real name; the real name of its executable; a
  DMA channel). CC0 removes all friction: anyone can merge, fork, translate, or
  re-share the compatibility database, which is exactly the community outcome we
  want, and it sidesteps sterile arguments about whether a facts-table is even
  copyrightable.
- **CC BY 4.0 — acceptable alternative.** If the project later wants attribution
  for the *curated selection and prose notes* (the arrangement and the
  human-written compatibility tips, which can attract thin copyright), CC BY 4.0
  preserves reuse while asking for credit. The trade-off is attribution friction
  on every downstream copy.
- **Decision:** ship `GAMES.INI` and profile data as **CC0-1.0**, recorded in the
  manifest, unless/until curated prose notes grow substantial enough to warrant
  CC BY 4.0 — a documented, deliberate switch, not a drift.
- **Hard boundary:** the database *describes* games; CASTALIA DOS **never** ships
  the games themselves — no copyrighted game binaries, no box art, no ROMs, no
  publisher assets. The DB points at software the user already, lawfully, has.

---

## 2.10 Release gate — the maintainer's pre-publish legal checklist

Run this **every time**, before any image (floppy, ISO, CF, `.img`) leaves the
project. Automate what can be automated in `scripts/`; the gate should be able to
**fail the build**. Do not publish with any step red.

**Step 0 — Freeze and pin.**
- [ ] Every third-party component pinned to an exact version/tag/commit.
- [ ] `third_party/MANIFEST.yml` updated to match the frozen bits.

**Step 1 — Provenance sweep (no Microsoft, no unknowns).**
- [ ] No MS-DOS/Microsoft binaries, message text, manuals, fonts, or logos on the
      image or in the tree.
- [ ] Every file on the target media traces to a manifest entry or to `src/`
      (Castalia MIT). No orphan/unknown binaries.

**Step 2 — Boundary check (copyleft containment).**
- [ ] No GPL/LGPL source under `src/`.
- [ ] No Castalia MIT `.EXE` statically links or embeds GPL code.
- [ ] Castalia↔FreeDOS interaction is interface/process/file only (§2.4).

**Step 3 — Per-component license verification.**
- [ ] For each manifest entry, the `spdx` field matches the **pinned version's own
      bundled license/readme** (re-read it; do not trust memory or this doc's
      table). Ambiguous → assume copyleft + source duty.
- [ ] Upstream copyright headers, `COPYING`/`LICENSE`, and readmes copied intact.
- [ ] Modified components carry change-notices with dates; patches are shipped.

**Step 4 — License-text completeness.**
- [ ] Every SPDX id in the manifest has its full text in `LICENSES/` **and** in
      `LICENSES\` on the media. No missing texts; no orphan texts.

**Step 5 — Source availability (GPLv2 §3(a)).**
- [ ] Complete corresponding source for **every** copyleft component shipped is
      present in the release (`\SOURCE` on image, or companion source bundle for
      floppy sets) at the pinned version, with patches and build recipe.
- [ ] Source rebuilds the shipped binaries (build recipe present and sane).

**Step 6 — Notice on media.**
- [ ] `THIRD-PARTY.txt` / `NOTICE` regenerated from the manifest, present at media
      root, accurate, includes source statement + trademark disclaimer.

**Step 7 — Trademark hygiene.**
- [ ] Product identified as "CASTALIA DOS"; no MS/Windows marks used as our own
      identity or logo; MS marks only descriptive/nominative; disclaimer present.
- [ ] No banner claims to *be* "MS-DOS 6.22"; compatibility wording used.

**Step 8 — Data & content clearance.**
- [ ] `GAMES.INI`/profiles licensed (CC0/CC BY) and recorded in the manifest.
- [ ] No copyrighted game binaries, box art, ROMs, or publisher assets shipped.

**Step 9 — Redistributability confirmation.**
- [ ] The §2.8 checklist passes in full.
- [ ] No component imposes non-commercial, no-derivatives, or field-of-use limits
      that would break redistribution.

**Step 10 — Record and sign off.**
- [ ] Tag the release; archive the manifest, `LICENSES/`, and source bundle with
      the image so the exact legal state of this build is reproducible forever.
- [ ] Maintainer records: "Release gate passed — <codename> <version>, <date>."

> **Gate rule:** any red box blocks publication. There is no "ship now, fix the
> license later." A CASTALIA DOS image is either fully clean and redistributable,
> or it does not ship.

---

## 2.11 Quick reference

| Question | Answer |
|---|---|
| Can we ship any MS-DOS file? | **No.** Never. Copyright + trademark. |
| Can we clone MS-DOS messages/manuals/logos? | **No.** Author our own; clean-room only from public interface specs. |
| What is the DOS base? | **FreeDOS** (GPLv2+ kernel + FreeCOM), plus open/free utilities. |
| Is there a special FreeDOS OEM contract? | **No.** Just honor each component's license (mostly GPL). |
| Does modifying the kernel spread GPL to our tools? | **No** — separate executables + interface-only use = aggregation. |
| How do we ship GPL source? | **§3(a): with the media** (`\SOURCE`), at the pinned version. |
| What license for Castalia code? | **MIT**. Docs: **CC BY 4.0**. `GAMES.INI`: **CC0**. |
| What protects the name/logo? | **Trademark**, held by the project; code stays free. |
| What is the final check before publishing? | The **release gate** (§2.10) — every box green or it does not ship. |

*End of Section 2 — Legal and Licensing Strategy.*
