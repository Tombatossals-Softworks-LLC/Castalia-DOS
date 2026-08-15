# src/setsound — SETSOUND.EXE (prototype)

> **Status:** an initial prototype is implemented in
> [`SETSOUND.C`](SETSOUND.C) and builds with `wmake setsound`. It offers
> the six sound profiles below, lets the user adjust port/IRQ/DMA, and
> writes `C:\CASTALIA\CFG\SOUND.BAT`. Patching `GAMES.INI` and card
> auto-probing are still to come.

Sound configuration helper. Writes `C:\CASTALIA\CFG\SOUND.BAT` (the
`SET BLASTER` / `SET SOUND` / `SET MIDI` lines that `AUTOEXEC.BAT` calls) and
can patch the `sound=` field of games in
[`../../config/GAMES.INI`](../../config/GAMES.INI).

Planned behaviour:

- Offer the sound profiles: None, PC Speaker, AdLib, SB 1.5, SB 2.0, SB Pro,
  SB 16 (Roland MT-32 / General MIDI later).
- For each, propose sane port/IRQ/DMA defaults and let the user override.
- Do a best-effort DSP-reset probe at ports 220/240 to confirm a card responds,
  while being honest that ISA detection is unreliable and manual override is the
  fallback.
- Write `SOUND.BAT` and update `CASTALIA.INI [sound]`.

Shares `../common/ini.*` and `../common/ui.*`. See
[`../../docs/SOUND.md`](../../docs/SOUND.md).
