# THE STORY — why CASTALIA DOS exists

Background and angle material for a feature. Quotes below are drawn from the
project's own design bible and may be attributed to "the Castalia DOS Project"
or "the project's design bible."

## The premise

There is no shortage of ways to run DOS in 2026. What there is a shortage of is a
DOS that treats a 30-year-old machine as something worth dressing well. Most
retro-DOS setups are a pile of drivers and a bare `C:\>` — functional, and
completely anonymous. CASTALIA DOS starts from the opposite instinct: that a real
386, booted on purpose, deserves a first-class, coherent, *dignified* face.

> *"A serious, elegant DOS-compatible environment for real machines — a
> Mediterranean fortress for your 386."*

## The fortress metaphor (functional, not decorative)

Castalia takes its identity from the **stone fortresses of Valencia and
Castellón** — Peñíscola on its sea-rock, Morella on its hill, the keep of
Sagunto. The metaphor is used for structure and reassurance, never narrated:

| Fortress idea | What it means in the product |
|---|---|
| The keep (*torreón*) | The core: kernel, shell, the Castalia main menu. |
| The walls | Compatibility and memory profiles that protect games. |
| The gate | The boot menu — you choose how you enter. |
| The armory | The tools: MEMPROF, SETSOUND, HWINFO, CFGEDIT. |
| The garrison | Safe Mode and the rescue tools that hold the line. |

Even the version codenames are fortress towns of the Valencian coast — Almenara,
Peñíscola, Morella, Tombatossals, Montornés — so the whole roadmap stays
thematically coherent, and legally safe (they're real, public places).

## The five words

The project describes its personality in five words:

> **Dignified · Precise · Warm · Sturdy · Unhurried.**

> *"Think of the feeling of a well-made IBM-era manual, a Castilian keep in warm
> afternoon light, and a technician who has fixed this exact problem a hundred
> times and is not the least bit worried. Castalia is the calm expert in the
> room."*

That voice shows up in small, real ways. Errors describe the situation and the
next step, and never blame the user: *"The drive is not ready. Insert a disk and
press R to retry"* — not *"Not ready reading drive A. Abort, Retry, Fail?"*

## The one rule that governs everything

> **Compatibility beats elegance.** *"When a choice must be made between a
> prettier design and a game that actually runs, the game wins. The elegance is
> in service of the machine, never the other way around."*

This is why the launcher unloads itself from memory before a game starts, why
there are eight memory profiles instead of one clever guess, and why the whole
project is gated on objective, testable behaviour — *it boots, it frees N KB, it
runs game X* — rather than on subjective polish.

## Legally clean on purpose

Castalia's other founding commitment is that it is a **serious, legally-clean**
DOS. It ships zero Microsoft material and builds only on FreeDOS and other
open/free components, with the copyleft carefully contained and every third-party
component inventoried with its licence and source. The project treats this as a
feature, not a footnote: a distribution you can lawfully redistribute worldwide,
forever.

## Why it's interesting to cover now

- It's a rare example of **taste applied to retro tooling** — design thinking,
  a real palette and voice, and a coherent identity, in 16 colours and 80
  columns.
- It's **honest engineering in the open**: every commit boots a real disk image
  in an emulator before it's accepted.
- It's a **clean-room, GPL-respecting** answer to "can retro-DOS be a *product*,
  not just a pile of drivers?"

The angle, in one sentence: *someone decided a 386 deserved a fortress, and then
built it — legally, testably, and beautifully.*

**Source & full design bible:** https://github.com/davabe/Castalia-DOS
