# CREDITS & LICENSING — CASTALIA DOS press kit

## Using these assets

You are welcome to use the materials in this press kit to **report on, review,
or write about CASTALIA DOS**. Specifically:

- **Text** in this `presskit/` folder (press release, fact sheet, boilerplate,
  feature list, technical notes, story, roadmap) is licensed **CC BY 4.0** —
  reuse and adapt it freely, with attribution to "The Castalia DOS Project."
- **Screenshots and logos** in `images/` may be reproduced in editorial coverage
  of CASTALIA DOS (articles, reviews, videos, social posts). Please don't alter
  the logo's proportions or colours, and don't use the Castalia keep or wordmark
  to imply endorsement of an unrelated product.
- Please **don't present CASTALIA DOS as your own** or as an official Microsoft,
  IBM, or FreeDOS product.

If in doubt, ask — details below.

## What CASTALIA DOS itself is licensed under

CASTALIA DOS is an **aggregation** of original work and open-source components:

- **Original Castalia code** (the ~20 tools, the UI toolkit, boot profiles,
  installer, configuration): **MIT**.
- **Castalia branding and artwork** (the keep, the wordmark, the palette, the
  copy): proprietary to the project; the text-mode logos are original work and
  are the ones provided in this kit.
- **FreeDOS components** (the kernel and FreeCOM shell, memory managers, base
  utilities): **GPL-2.0-or-later** and other free licences, kept in the
  project's `third_party/` tree and shipped **with their source**.
- Documentation in the project: **CC BY 4.0**.

The full licence texts live in the repository's `LICENSES/` folder.

## Attribution & honesty

CASTALIA DOS is **built on FreeDOS** and attributes it openly — in the shipped
source, in the licence tree, and on the system's own **About** screen ("Built on
FreeDOS (GPLv2+) … source in third_party"). CASTALIA DOS is **not** an official
FreeDOS product and does not imply the FreeDOS Project's endorsement.

CASTALIA DOS contains **no Microsoft code, text, fonts, or branding**. Any
"MS-DOS-compatible" wording is a plain, factual description of behaviour, never a
claim of origin or a use of Microsoft's marks.

## Credits

- **Tombatossals Softworks / The Castalia DOS Project** — original tools,
  installer, boot profiles, branding, documentation, and this press kit.
  (Repository owner: `davabe`.)
  - **Created by:** Dave Abellan (creator & programmer) and Claudio di Castello.
- **The FreeDOS Project** (Jim Hall and contributors) — the kernel and shell
  CASTALIA DOS builds on.
- **Open Watcom contributors** — the 16-bit C compiler and linker used to build
  the Castalia tools and rebuild the kernel.
- The authors of the open memory managers and utilities Castalia bundles
  (HIMEMX, JEMM386, CTMOUSE, SHSUCDX, UIDE, and others), each under its own
  licence.

## How the images were made

The screenshots and logos in this kit are rendered from the **same CP437 keep
art the DOS binaries draw** (`src/common/LOGO.C`), in the authentic VGA
16-colour palette, so they match what the hardware actually paints. This keeps
the kit faithful and reproducible.

## Contact

- **Studio:** Tombatossals Softworks
- **Press contact:** hello@tombatossalssoftworks.com
- **Project & source:** https://github.com/davabe/Castalia-DOS
- **Questions, corrections, or asset requests:** open an issue or discussion on
  the repository.

*CASTALIA DOS is a project of Tombatossals Softworks. © 2026.*
