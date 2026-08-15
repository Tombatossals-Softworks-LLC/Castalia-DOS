# PRESS RELEASE

**For immediate use · Tombatossals Softworks · 2026**

---

## CASTALIA DOS opens the gate: a dignified, game-focused DOS for real 386 hardware

**A new, legally-clean, open-source DOS environment gives vintage PCs a serious
face — a Mediterranean fortress for your 386 — with eight memory profiles, a
game launcher, an installer, and a boot experience that finally looks like it
was *designed*.**

The retro-PC scene has never lacked ways to run DOS. What it has lacked is a DOS
that treats a 30-year-old machine as something worth dressing well. **CASTALIA
DOS** is that project: a curated, bootable DOS-compatible operating environment,
built on the open-source **FreeDOS** kernel and shell, wrapped in an original
suite of tools, boot profiles, a game launcher, an installer, help system, and a
coherent 80×25 text-mode identity inspired by the stone fortresses of the
Valencian coast.

The design goal is unusually strict: when a Castalia machine powers on, it opens
onto **Castalia and nothing else** — no scrolling kernel banners, no compiler
credits, no wall of licence text. The fortress keep rises from the ground, the
**CASTALIA DOS** wordmark lights up amber, stars twinkle over a Mediterranean-blue
field, and within about three seconds the menu is ready. Every screen — the boot
banner, the rescue disk, the installer, the main menu — wears the same face.

### Built for metal, not nostalgia theatre

Castalia targets **real hardware first**: 386SX, 386DX, 486, and early Pentium
machines with limited RAM, VGA, a serial or PS/2 mouse, IDE or CompactFlash
storage, and AdLib/Sound Blaster audio. It runs cleanly in DOSBox-X and 86Box
too, but the north star is a physical 386 booting unattended from cold.

Its most practical feature is memory. Castalia ships **eight boot profiles** —
Maximum Compatibility, XMS Gaming, EMS Gaming, CD-ROM Gaming, Windows 3.x,
Diagnostics, Safe Mode, and a plain command prompt — each tuned to hand a game
the most conventional memory it can get for the job. Around that sit original
text-mode tools: a game launcher that unloads itself before the game runs, a
memory-profile switcher, sound configuration, hardware diagnostics, a config
editor, rescue utilities, a file manager, a disk surface scanner, a benchmark,
and a small set of built-in minigames.

### Legally clean by construction

Castalia contains **no Microsoft code, text, or branding**. It is FreeDOS plus
original, MIT-licensed Castalia work; the GPL FreeDOS components live in a
`third_party/` tree and ship with their source. The one modification Castalia
makes to the FreeDOS binaries — rebranding the boot sign-on to read *CASTALIA
DOS* — is done by rebuilding the FreeDOS kernel from its own published source
with a one-line change, and is documented in the project's licensing plan.
FreeDOS is attributed honestly throughout, including on the system's own About
screen.

### Availability

CASTALIA DOS is **open source and in active development**. The source, build
scripts, and continuous-integration pipeline — which builds the 16-bit tools
with Open Watcom and boot-tests a real floppy image in an emulator on every
change — are public at **https://github.com/davabe/Castalia-DOS**. The flagship
**386SX Edition** is on the road to its **1.0 "Tombatossals"** release; 486 and
Pentium editions are planned.

> *"Castalia is the calm expert in the room. It respects that the person on the
> other side is running a 30-year-old machine on purpose, and knows exactly what
> they're doing."* — from the project's design bible.

**Learn more, see the source, or file a question:**
https://github.com/davabe/Castalia-DOS

**Press contact:** hello@tombatossalssoftworks.com

*Screenshots, logos, and boilerplate are in this press kit. CASTALIA DOS is a
project of Tombatossals Softworks. © 2026.*
