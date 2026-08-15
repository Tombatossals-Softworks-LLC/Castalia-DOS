# src/gamecfg — GAMECFG.EXE (prototype)

> **Status:** implemented in [`GAMECFG.C`](GAMECFG.C); builds with
> `wmake gamecfg`. Compiles clean as C89.

The game-database editor. It loads every game from `GAMES.INI`, shows them in
a list, and lets you **add**, **edit**, or **delete** entries in a form —
then writes the whole file back. `LAUNCH.EXE` reads the same file, so this is
the launcher's editor.

Form fields and how they are edited:

| Field | Edit method |
|---|---|
| Name, Path, Exe, Args, Notes | inline text editor (Enter to edit) |
| Profile (`CLEAN/XMS/EMS/CDROM`) | Left/Right cycle |
| Sound (`NONE/SPKR/ADLIB/SB/SBPRO/SB16`) | Left/Right cycle |
| Mouse, CD required | Left/Right or Enter toggles yes/no |

Behaviour:

- **List view:** ↑/↓ move, Enter edit, `A` add a new game, Del delete, F2 save,
  Esc quit (prompts to save if there are changes).
- **Add** creates a stub game and generates a unique section id from the name
  (`A-Z0-9`, de-duplicated).
- **Save** backs up the existing `GAMES.INI` to `C:\CASTALIA\BACKUP` first,
  then rewrites the file with a fresh header comment and every entry.

Note: because it rewrites the file from the parsed entries, hand-written
comments in `GAMES.INI` are not preserved across a save — the per-game data is.

Shares `../common/ini.*` and `../common/ui.*`. See
[`../../docs/LAUNCHER.md`](../../docs/LAUNCHER.md).
