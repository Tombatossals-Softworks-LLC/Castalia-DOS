# src/memprof — MEMPROF.EXE (prototype)

> **Status:** an initial prototype is implemented in
> [`MEMPROF.C`](MEMPROF.C) and builds with `wmake memprof`. It reads
> `PROFILES.INI`, shows each profile's details and the machine's live
> conventional-memory total and largest free block, and marks the current
> profile. Adjusting `MENUDEFAULT` in `CONFIG.SYS` is still to come; for
> now switching profiles is done by rebooting and choosing.

Memory-profile switcher and inspector. Reads
[`../../config/PROFILES.INI`](../../config/PROFILES.INI) to describe each boot
profile (free conventional target, XMS/EMS/UMB, example games) and shows the
machine's current memory picture (like a friendly `MEM /C`).

Planned behaviour:

- List the eight profiles with their free-conventional targets and trade-offs.
- Show the currently booted profile (`%CASTPROFILE%`) and live free memory.
- Optionally adjust `MENUDEFAULT` in `CONFIG.SYS` to change the default profile
  for the next boot (with a backup to `C:\CASTALIA\BACKUP` first).
- Explain that switching profiles requires a reboot (real-mode DOS cannot
  re-layout drivers live).

Shares `../common/ini.*` and `../common/ui.*`. See
[`../../docs/MEMORY.md`](../../docs/MEMORY.md).
