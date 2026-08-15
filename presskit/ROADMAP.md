# ROADMAP — where CASTALIA DOS is going

Direction and ambition, in press-friendly form. These are **milestones, not
calendar dates**: CASTALIA DOS is built by a very small team on open-source
foundations, and retro/DOS work is deliberately slow and heavily tested.

## What we want to achieve

- A **stable, legally-clean, genuinely pleasant** DOS environment that a real
  386-class machine can boot unattended, install from a floppy, and run games on
  — with the most conventional memory the hardware can give.
- A **coherent identity** across every screen: one fortress, one voice, one
  palette, from the boot banner to the smallest error message.
- A project that is **verifiable in the open** — every change boots a real disk
  image in an emulator before it ships — and eventually **validated on metal**
  (386SX/486 hardware, Sound Blaster IRQ/DMA, IDE/CompactFlash).

## The releases (codenames are fortress towns of the Valencian coast)

| Version | Codename | Theme — what it delivers |
|---|---|---|
| 0.1 | **Almenara** | *It boots.* The minimal FreeDOS+Castalia bootable image. |
| 0.2 | **Peñíscola** | Boot profiles, the main menu, the game launcher, sound / CD-ROM / mouse. |
| 0.5 | **Morella** | The installer and a game-compatibility database. |
| 1.0-rc | **Tombatossals** | Real-hardware validation and a public alpha. |
| **1.0** | **Tombatossals** | The stable release. The keep stands. |
| 1.1 | **Montornés** | File manager, refinements, and early original-kernel research. |

Almenara is named for the 386SX-era simplicity of the first bootable image;
**Tombatossals**, the legendary giant-hero of Castellón folklore, is deliberately
reserved for the big, stable, complete **1.0**.

## Beyond 1.0

- **More editions by hardware class.** The flagship 386SX Edition is tuned to be
  correct on the humblest target; **486** and **early-Pentium** editions would
  add more RAM and CD-ROM headroom and larger game sets. Edition names describe
  the *floor*, not a lock — the 386SX Edition already runs happily on a 486.
- **A deeper game-compatibility database**, so the right memory profile is
  suggested per title.
- **Exploratory only, and clearly optional:** research into an original
  DOS-compatible kernel — a multi-year effort that must never block a shippable
  product, and only starts after 1.0 is stable, if there is ever a concrete
  reason FreeDOS cannot serve.

## The sequencing philosophy

The order is not arbitrary. Two things are front-loaded because they are cheap to
get wrong and expensive to fix later: **legal cleanliness** (the licence audit
and third-party separation exist *before* the first component is vendored) and
**bootability on real hardware** (a DOS that does not boot is not a DOS). Polish
comes only after a machine actually boots Castalia from cold.

**Follow progress:** https://github.com/davabe/Castalia-DOS
