# src/help — HELP.EXE (prototype)

The help-system reader designed in [`../../docs/HELP.md`](../../docs/HELP.md)
(`wmake help`). Reads `HELP.IDX` (plain INI via the shared reader) from
`C:\CASTALIA\HELP\` — with dev-tree fallbacks — shows the topic index, and
pages each pre-wrapped 80-column `.TXT` page full-screen.

Usage: `HELP` opens the index; `HELP SOUND` jumps straight to a topic by id
or unique prefix (an unknown name drops to the index with a notice). The
Castalia menu's Help item runs it when installed. New pages need no
recompile: add a `.TXT` and one `HELP.IDX` line.

Links `ui` and `ini`.
