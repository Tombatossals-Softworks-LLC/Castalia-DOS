# src/setup — SETUP.EXE (prototype)

> **Status:** an initial prototype is implemented in [`SETUP.C`](SETUP.C)
> and builds with `wmake setup`. It runs the full wizard (Welcome →
> Target → Options → Confirm → Install → Done) and performs the real,
> safe work: backing up existing config, creating the tree, copying the
> DOS core and Castalia layer (via `XCOPY`), writing the boot files and a
> sound profile, and — with confirmation — running `SYS` to make the disk
> bootable.
>
> **Safety:** the prototype deliberately does **not** partition or format.
> It assumes the target is an already-formatted FAT16 disk (a fresh
> `FORMAT`, or an existing DOS). Auto-partition/format is intentionally
> left to the user with `FDISK`/`FORMAT`. Still to come: in-wizard disk
> detection, the multi-floppy source flow, and emergency-boot-disk
> creation.

The CASTALIA DOS installer. Installs to a FAT16 hard disk or CompactFlash card
from the boot floppy or an existing DOS system, backing up any existing
`CONFIG.SYS`/`AUTOEXEC.BAT` first and making the target bootable.

The full design — flow, UI mockups, required files, disk layout, floppy-set
strategy, safety checks, rollback plan, and error messages — is in
[`../../docs/INSTALL.md`](../../docs/INSTALL.md).

The wizard is framed by two full-screen fortress splashes: a **Welcome**
screen where the keep and the `CASTALIA DOS` wordmark greet the user, and a
celebratory **INSTALLED** screen at the end. Both share the keep and block-
letter wordmark from `../common/logo.*`, so the installer wears the same face
as the boot banner. The install phase shows a framed, numbered progress log
and a final completion bar.

Shares `../common/ui.*` and `../common/logo.*`. Because installation is the one
destructive operation in the whole system, this tool is written last (after the
bootable core and the everyday tools are proven) and tested hardest.
