# src/games — the Castalia minigames

The built-in games, registered in `config/GAMES.INI` so the launcher lists
them like any other title. All: text mode, BIOS-tick pacing, keyboard-only,
no TSRs, instant exit, C89, link `ui` only.

- **`SNAKE.C` → SNAKE.EXE** (`wmake snake`) — classic snake with incremental
  drawing (only head/tail cells repaint per step, so a 386SX stays smooth),
  amber apples, speed-up every 5 apples, pause, crash flash, restart.
- **`PUZZLE.C` → PUZZLE.EXE** (`wmake puzzle`) — the 15-puzzle; shuffled by
  400 random legal moves (always solvable), move counter, green numbers on
  tiles already home, win panel.
- **`ALMENA.C` → ALMENA.EXE** (`wmake almena`) — falling blocks, castle
  style ("almena" = battlement merlon): seven pieces with generated
  rotations and a simple wall-kick, soft/hard drop, line flash and collapse,
  levels every 10 lines, next-piece preview, pause, restart. Incremental
  drawing; the well repaints fully only after a line clear.
- **`MINAS.C` → MINAS.EXE** (`wmake minas`) — minesweeper ("chart the
  moat"): three difficulties (9×9/10 to 30×16/99), cursor play, classic
  colour-coded numbers, flags, a live timer, iterative (stack-based) flood
  reveal, and the first-reveal-is-always-safe rule (mines are placed after
  the first move).

- **`SIEGE.C` → SIEGE.EXE** (`wmake siege`) — "Asedio", a catapult duel:
  two Mediterranean keeps on a jagged skyline trade boulder fire. Set angle
  and power, read the wind, and breach the enemy keep first (best of three).
  A self-made integer sine table and 32-bit-`long` fixed-point ballistics
  (no floating point), an animated arc, and an adaptive CPU gunner that
  brackets its power interval to zero in over successive shots.
- **`REVERSI.C` → REVERSI.EXE** (`wmake reversi`) — Othello against the
  machine: full legal-move/flip rules, legal moves marked for the human,
  flip animation, pass handling, and a negamax + alpha-beta opponent
  (depth 4) with a corner-hungry positional weight table; evaluation stays
  small enough to be exact in a 16-bit int (no `long` needed).
- **`BARRELS.C` → BARRELS.EXE** (`wmake barrels`) — Sokoban in the fortress
  cellar: eight hand-crafted, verified-solvable chambers; push (never pull)
  every barrel onto its mark; bounded undo that correctly reverses a push,
  per-level reset, and move/push counters.

- **`SOLITARE.C` → SOLITARE.EXE** (`wmake solitare`) — draw-one Klondike:
  stock/waste, four foundations, seven tableau columns; a cursor picks up
  a card or a face-up run and drops it where the rules allow (foundations
  up by suit, tableau down in alternating colour, only Kings to an empty
  column), auto-flip on exposure, unlimited stock recycling, an `A`
  safe-autoplay-home key, and a win panel at 52 cards.

Link `ui` only.
