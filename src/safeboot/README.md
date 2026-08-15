# src/safeboot — SAFEBOOT.EXE (prototype)

> **Status:** implemented in [`SAFEBOOT.C`](SAFEBOOT.C); builds with
> `wmake safeboot`. Compiles clean as C89.

The rescue tool, run from Safe Mode or the emergency boot floppy to repair a
system whose boot files are broken. It shows the status of the current and
backed-up `CONFIG.SYS`/`AUTOEXEC.BAT` and offers three actions:

1. **Restore from backup** — copy `C:\CASTALIA\BACKUP\{CONFIG.SYS,AUTOEXEC.BAT}`
   back to the root.
2. **Save current to backup** — copy the live boot files into the backup folder.
3. **Write a minimal safe config** — write a guaranteed-bootable minimal
   `CONFIG.SYS`/`AUTOEXEC.BAT` (HIMEMX + `DOS=HIGH`, a plain prompt), after
   first backing up whatever is there.

File copies are done in C (not via the shell) so the tool works even when the
command environment is fragile. The system drive is assumed to be `C:`. Pairs
with `CFGEDIT.EXE` (edit) and the emergency boot disk. See
[`../../docs/HELP.md`](../../docs/HELP.md) (`RECOVERY.TXT`) and
[`../../docs/BOOT.md`](../../docs/BOOT.md).

Shares `../common/ui.*`.
