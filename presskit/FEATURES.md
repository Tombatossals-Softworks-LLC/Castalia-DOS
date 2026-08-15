# FEATURES — CASTALIA DOS

Plain-language feature list for coverage. Grouped by what a reader would care
about.

## The boot experience

- **CASTALIA-only sign-on.** The FreeDOS kernel is rebuilt from source so its
  boot banner reads *CASTALIA DOS 386SX Edition* — no "FreeDOS kernel", no
  "Open Watcom", no scrolling licence wall.
- **The fortress keep.** An animated splash: a three-tower keep rises from the
  ground, the **CASTALIA DOS** wordmark lights amber, the tower pennants and lit
  windows flicker like candlelight, and stars twinkle over the Mediterranean-blue
  field — all in about three seconds. Any key skips it; it never blocks the boot.
- **One face everywhere.** The same keep and wordmark appear on the boot banner,
  the rescue/install floppy, the installer, and the main menu.

## Memory profiles (the practical heart)

- **Eight boot profiles**, chosen at a clean menu at boot:
  1. **Maximum Compatibility** — HIMEM only, most free conventional RAM.
  2. **XMS Gaming** *(default)* — HIMEM + UMBs, no EMS page frame.
  3. **EMS Gaming** — HIMEM + EMS page frame + UMBs.
  4. **CD-ROM Gaming** — CD-ROM stack + sound + UMBs.
  5. **Windows 3.x Mode** — EMS + a SmartDrive-style cache.
  6. **Diagnostics** — boots, then runs the hardware report.
  7. **Safe Mode** — bare kernel, no drivers, for repair.
  8. **Command Prompt Only** — no menu, just a prompt.
- **Maximum conventional memory by design.** The shared boot block is kept lean,
  DOS relocates to the HMA where possible, and the default profile targets the
  most free conventional RAM a game is likely to need.

## The Castalia tool suite (original, text-mode)

- **Main menu (`CASTALIA`)** — the keep: a fast 80×25 front end with submenus.
- **Game launcher (`LAUNCH`)** — browse and start games; it and the menu unload
  from memory *before* the game runs, so the game gets every free byte.
- **Memory-profile switcher (`MEMPROF`)** — change profiles without editing files.
- **Sound setup (`SETSOUND`)** — pick card, IRQ, DMA; writes the sound environment.
- **Hardware diagnostics (`HWINFO`)** — CPU, memory, ports, and devices at a glance.
- **Config editor (`CFGEDIT`)** — edit CONFIG.SYS / AUTOEXEC safely.
- **Rescue (`SAFEBOOT`)** — restore a known-good boot from backup, or re-SYS a disk.
- **File manager (`CASTFM`)** — browse and manage files.
- **Disk doctor (`CASTDOC`)** — read-only disk surface verification.
- **Benchmark (`CASTMARK`)** — a period-appropriate system benchmark.
- **Copy/transfer (`CASTCOPY`)** — diskette rescue and transfer.
- **Text editor (`CASTEDIT`)** and **help reader (`HELP`)**.
- **Minigames** — Snake, 15-Puzzle, Almena (falling blocks), and Minas
  (minesweeper), for when a boot disk needs something to do.

## Install & rescue

- **Safe installer (`SETUP`).** A text-mode wizard — Welcome → Target → Options →
  Confirm → Install → Done. It never partitions or formats on its own, backs up
  any file it replaces, and changes nothing until you confirm.
- **One disk, two jobs.** The 1.44 MB floppy is the installer's first disk *and*
  a self-contained emergency boot/rescue disk.

## Identity & polish

- **A dignified voice.** Calm, complete sentences; never blames the user, never
  shouts. Errors explain what happened and the next step.
- **A coherent 16-colour palette** — amber accent on Mediterranean blue, steel-
  gray panels — applied consistently across every screen.
- **A text-mode keep logo** drawn entirely in CP437 glyphs, so it renders
  identically on colour VGA and on monochrome amber/green displays.

## Legally clean

- **No Microsoft material** — no MS-DOS code, binaries, message text, or branding.
- **FreeDOS used under its own licence**, honoured in full, with source shipped.
- **Copyleft contained** — GPL components stay as separate executables in
  `third_party/`, aggregated with, not linked into, Castalia's MIT code.
