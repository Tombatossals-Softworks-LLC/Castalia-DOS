# src/cfgedit — CFGEDIT.EXE (prototype)

> **Status:** implemented in [`CFGEDIT.C`](CFGEDIT.C); builds with
> `wmake cfgedit`. Compiles clean as C89.

A small, self-contained **line editor** for `CONFIG.SYS` and `AUTOEXEC.BAT`.
It is line-oriented rather than a full free-text editor, which keeps it tiny
and correct: pick the file, navigate lines, edit the current line in place,
insert or delete whole lines, and save.

Key safety feature: **on save it first copies the existing file to
`C:\CASTALIA\BACKUP`**, so any edit can be undone with `SAFEBOOT.EXE`. Because
it depends on no external editor, it still works from the rescue floppy.

Controls: ↑/↓ move, Enter edit the current line, Ins add a line, Del remove a
line, F2 save (with auto-backup), Esc quit (prompts if there are unsaved
changes). The inline single-line editor supports Home/End, Left/Right,
Backspace, Delete, and horizontal scroll for long lines.

Limits (documented, not silent): up to 300 lines of up to ~160 characters; a
longer file is loaded up to the cap with a visible warning. These are ample
for real `CONFIG.SYS`/`AUTOEXEC.BAT` files.

Shares `../common/ui.*`.
