# src/castedit — CASTEDIT.EXE (prototype)

The suite's general text editor (`wmake castedit`): CFGEDIT's proven
line-oriented engine, generalised to **any file**. Open from the command
line (`CASTEDIT NOTES.TXT`) or via an in-program prompt; a name that does
not exist starts a new file.

Keys: ↑/↓ move, Enter edit the current line (shared `ui_editline`), Ins/Del
insert/remove lines, **F2 save**, **F4 save-as**, Esc quit (prompts on
unsaved changes).

Safety: every save first copies the previous version to a `.BAK` beside the
file (`NOTES.TXT` → `NOTES.BAK`), so saves are reversible. Caps (400 lines
× 160 chars, no heap) are visible, not silent. Menu item 3.

Links `ui` only.
