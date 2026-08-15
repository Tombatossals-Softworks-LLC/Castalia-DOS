# src/castfm — CASTFM.EXE (prototype)

> **Status:** implemented in [`CASTFM.C`](CASTFM.C); builds with
> `wmake castfm`. Compiles clean as C89. This is the "1.1" tool from
> [`../../docs/FILE-MANAGER.md`](../../docs/FILE-MANAGER.md), kept minimal.

A light, single-pane, keyboard-first file manager (aliases `CFM.EXE`,
`ALCAZAR.EXE`). It lists a directory, enters subdirectories, changes drives,
shows free space, views text files, runs programs, and does copy / move /
delete / make-directory.

Keys:

| Key | Action |
|---|---|
| ↑/↓, PgUp/PgDn, Home/End | move the selection |
| Enter | open a directory, or view a file |
| F3 | view the selected text file |
| F4 | run the selected program (`.EXE`/`.COM`/`.BAT`) |
| F5 / F6 | copy / move (prompts for destination) |
| F7 | make a directory |
| F8 | delete the selected file (files only; refuses directories) |
| F9 | change drive |
| F10 / Esc | quit |

Technical notes:

- Directory listing is portable across compilers: Turbo C `findfirst`/`ffblk`
  and Open Watcom `_dos_findfirst`/`find_t` are both handled; drive and
  free-space queries use `INT 21h` (AH=0Eh / AH=36h) directly.
- Fixed 512-entry directory array and a 400-line text viewer buffer (no heap);
  overflow is capped, not crashed.
- `F4 Run` uses `system()` (the file manager stays resident during the child);
  for maximum free memory when launching games, use the game launcher
  (`LAUNCH.EXE`), which unloads first via the batch handoff.
- Deleting directories is intentionally not offered, to avoid accidental
  recursive loss.

Shares `../common/ui.*`.
