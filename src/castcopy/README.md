# src/castcopy — CASTCOPY.EXE (prototype)

Diskette rescue & transfer (`wmake castcopy`): browse A:/B: (with subdirs),
tag files with `Space` (or `*` for all), set the destination with `F4`
(default `C:\GAMES`), then `F5` copies with per-file and total progress
bars and live KB/s. A **read-back verify pass is ON by default** — this tool
exists to pull data off aging diskettes, so a verify mismatch or write error
deletes the partial destination file rather than leaving a silent bad copy.

Keys: `Space/Ins` tag · `*` all · `Enter` open dir · `Backspace` up ·
`A`/`B` drive · `F4` dest · `V` verify toggle · `F5` copy · `Esc` quit.
Overwrite prompts offer Y/N/All/stop. Restores the original drive on exit.

Links `ui` and `dirw` (shared portable directory scanning).
