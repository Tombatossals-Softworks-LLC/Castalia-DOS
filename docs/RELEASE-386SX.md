# Testing CASTALIA DOS on a real 386SX

This is the guide for the **hardware validation builds** on the way to 1.0.
Per [`ROADMAP.md`](ROADMAP.md) §21.2 those builds carry the Tombatossals
codename with a pre-release suffix precisely because hardware sign-off has not
happened yet. **You are the hardware sign-off.**

Current build: **`1.0-alpha5 "Tombatossals"`**.

The git tag for alpha5 went out misspelled, as `v1.0-aplha5`, and points at
`2d69bc7`. It stays as published; later tags follow
[`DISTRIBUTION.md`](DISTRIBUTION.md): annotated, `v<version>-<codename>`.

---

## What changed since alpha1/alpha2

The first hardware run found four defects in one screenshot. All four are
fixed, and each one is now guarded by a check in `scripts/check.sh`.

| What you saw | What was wrong |
|---|---|
| `HimemX: XMS is already installed` six times, `An EMM is already installed` | `CONFIG.SYS` was written in MS-DOS multi-config syntax (`[MENU]`, `MENUITEM`, named `[blocks]`). **The FreeDOS kernel implements none of it** — grep its source for `MENUITEM` and there is nothing. So no line was gated to a profile: every profile's drivers loaded on every boot. Rewritten in the kernel's own syntax (`MENU` lines, `MENUDEFAULT`, `n?` prefixes). |
| `No label specified for GOTO.` | Same cause. `%CONFIG%` is set by the kernel to the menu **digit**, and only when a real menu runs — with no menu it was empty, so `GOTO %CONFIG%` had nothing to jump to and the batch file stopped dead. That is why you never saw the banner or the menu. |
| `Can not redirect output to file ']'.` | A comment. FreeCOM scans a line for `>` `<` `|` *before* it notices the line is a `REM`, and one comment read `[CASTALIA C:\>]`. `GAMES.BAT` had the same bug in an arrow, which would have fired on every game launch. |
| `←[1;33m←[0;37mCASTALIA C:\…` | The prompt used ANSI colour escapes, but no profile loads an ANSI driver. The prompt is plain ASCII now: `[CASTALIA C:\]>`. |

One more defect was found while fixing those, and it would have broken exactly
the test you are here to run: **`UIDE.SYS` never loaded.** Its default cache is
20 MB of XMS and its smallest is 5 MB, so on a 4 MB 386SX it fails with
`XMS init error; UIDE not loaded!` and no CD-ROM appears at all. It now runs
`/N1 /N3` — CD/DVD drives only, no XMS cache — which needs `DEVICE=` rather
than `DEVICEHIGH=`.

---

## What is already verified, and what is not

Being clear about this matters more than being encouraging: the point of your
test is to find what emulation could not.

**Verified, by booting the real image in QEMU:**

- The floppy boots to `AUTOEXEC.BAT` from cold.
- `SETUP` installs to the hard disk, and the **installed** system boots to the
  eight-entry menu. Each of the eight was selected in turn and screenshotted:
  the highlight bar tracks the choice, only that profile's drivers load, the
  10-second timeout auto-boots the default, and `CASTID`/the menu status bar
  report the matching profile name — which they read from the kernel, not from
  a variable.
- The CD-ROM chain works against an emulated ATAPI drive: `UIDE` finds it,
  `SHSUCDX` assigns `D:`, and `DIR D:` lists a data disc.
- The Castalia kernel answers its identity calls — build, edition `01h`, OEM id
  `CAh`, the boot profile from `CASTALIA=`, and the boot tick behind the uptime
  readout. A stock FreeDOS kernel fails the same test, so it is a real check.
- Every tool compiles for 8086-class real mode with Open Watcom and carries a
  valid MZ header.

**Verified on a real 386SX, 2026-10-01** (photos and numbers in
[`tests/results/2026-10-01-dabellan-386sx-real.md`](../tests/results/2026-10-01-dabellan-386sx-real.md)):

- The installed system boots from `C:` to the Castalia menu on the CLEAN and
  EMS profiles, and the status bar names the profile.
- The EMS profile gets a working page frame: XMS 3.00 and EMS are both
  present. This is what QEMU could not show.
- The kernel identity calls answer on real hardware: `CASTID` shows build 1,
  edition `01h`, OEM `CAh`, the boot profile and a live uptime.
- `HWINFO` reports the 80386, the 387, 639 KB base memory, XMS, EMS, VGA and
  the mouse driver.
- `CASTMARK` completes the CPU, FPU, memory and video benchmarks.
- A VGA game, *The Secret of Monkey Island*, starts from Launch Games and runs.

One failure: the `CASTMARK` disk benchmark stops with
`Error writing to drive C: DOS area: drive not ready`. The benchmark writes its
test file before reading it, and that write is what fails. Whether ordinary
writes to `C:` fail on this machine too is the next thing to find out.

**Worth re-running on the 386SX with the next build.** None of these has met
real hardware yet:

1. At the prompt, before anything else: `CHKDSK C:` (no `/F`), then
   `ECHO test > C:\T1.TXT` on CLEAN and on SAFE. If that fails the same way,
   the problem is the disk path, not the benchmark.
2. `CASTMARK` again. A disk error should now read `failed: drive not ready`
   in the marks panel instead of DOS's Abort/Retry prompt, and the index
   should still appear from the other four benchmarks. The test file now
   goes to `%TEMP%` (`C:\TEMP`).
3. Any tool after a game that leaves a graphics mode: the menu should come
   back in text mode on its own.
4. `CASTDOC` on a 720 KB disk in a 1.44 MB drive: it should scan at 9 sectors
   per track and say it took the geometry from the boot sector.
5. `SETUP` on a disk that already has Castalia: it should keep your INI files
   and leave `CONFIG.ORG`/`AUTOEXEC.ORG` in `C:\CASTALIA\BACKUP` untouched.

**Not verified anywhere — this is your list:**

| Area | Why emulation could not settle it |
|---|---|
| **CD audio** (`CDPLAYER`) | QEMU does not implement CD-DA audio at all. The whole audio path is written to spec and has never met a real drive. |
| **A real CD-ROM drive** | The data path works against QEMU's ATAPI model. Real drives, real controllers and a 386SX's ISA IDE are a different matter. |
| **Sound Blaster** | IRQ/DMA behaviour is exactly what emulators smooth over. |
| **PC speaker** | Emulated coarsely; it needs an ear and a real cone. |
| **`CASTMARK` disk anchor** | An emulated IDE image is not a real drive or a CompactFlash card. On the first real 386SX the benchmark failed writing its test file to `C:`, so there is still no real number. |
| **Real IDE / CompactFlash** | Timing and geometry quirks are hardware-specific. |
| **`CASTLINK`** | Needs a second machine and a null-modem cable. |
| **`UNDEL`** | Needs a diskette with genuinely deleted files. |

---

## Writing the image to a floppy

The file is a raw 1.44 MB image — a byte-for-byte diskette, not an archive.

```sh
# Linux / macOS  (check the device name twice; this overwrites it)
sudo dd if=castalia-dos-1.0-alpha5-tombatossals-boot.img of=/dev/fd0 bs=512 conv=fsync
```

On Windows use **RawWrite** or **Rufus** in DD mode. Verify the checksum first
against the `.sha256` file shipped beside the image.

If the 386SX has no working 3.5″ drive, the image also boots from a
CompactFlash card written the same way, provided the BIOS can boot it.

---

## First boot from the floppy

You should see the fortress keep, the `CASTALIA DOS` wordmark, and a rescue
panel offering `SETUP`, `SAFEBOOT` and `HWINFO`, with an `A:\>` prompt.

Two things worth doing immediately:

1. **`CASTID`** — the signature card. It should say `CASTALIA build 1`,
   `OEM identity CAh`, `Boot profile SAFE`, and a live uptime. If it says
   "stock DOS kernel", the wrong `KERNEL.SYS` got onto the disk and everything
   below is moot.
2. **`HWINFO`** — confirm it identifies your CPU as 80386 and sees your memory,
   floppy drives and ports correctly. On a 386SX the CPU row reading
   "80386 (SX/DX look alike to software)" is *correct*, not a bug: no
   instruction distinguishes an SX from a DX. The narrower panels in `CASTID`
   and `CASTMARK` show the same thing as "80386 (SX or DX)".
   The **Video adapter** row now names what it found — VGA, EGA, CGA or MDA —
   rather than only answering "is it VGA?". If it disagrees with the card you
   know is fitted, that is worth reporting: only the VGA path has met a
   running machine so far.

The rescue floppy always boots the `SAFE` profile and has no menu — a rescue
disk must not depend on a hard disk or on a choice. The eight-profile menu
lives on the installed system.

---

## Installing, and the boot menu

Run **`SETUP`** and let it install to `C:`. It backs up any existing
`CONFIG.SYS`/`AUTOEXEC.BAT` to `C:\CASTALIA\BACKUP` first. If the target drive
is not there or not formatted, SETUP now says so before touching anything and
tells you the `FDISK` → `FORMAT C: /S` → `SETUP` order.

Remove the floppy and reboot. You should get a **blue menu screen** with eight
entries, entry **2 (XMS Gaming)** highlighted and a countdown from 10.

What to check here:

- Arrows move the highlight bar; a digit jumps straight to an entry; Enter
  accepts; leaving it alone boots entry 2.
- Only the chosen profile's drivers scroll past. Seeing `HimemX` or `Jemm386`
  load **twice** is the old bug and means the wrong `CONFIG.SYS` is on `C:`.
- The Castalia menu's status bar, bottom left, should name the profile you
  picked. That value comes from the kernel via `INT 2Fh`, so it proves the
  `CASTALIA=` directive tracked your choice, not just that a batch file guessed.

---

## The CD-ROM test

This is the part I most need reported back. The data path now works against an
emulated drive; a real drive on a real ISA IDE controller is the open question,
and **CD audio has never run anywhere**.

1. Boot and choose **`4. CD-ROM Gaming`**.
2. During boot, watch for two lines:
   - `UIDE` reporting what it found — it prints one `CD0:` line per drive,
     naming the controller position and the drive's own model string;
   - `SHSUCDX` binding that device to a drive letter (`/L:D`, so `D:`).
3. At the prompt, check the drive is there: `DIR D:` with a data CD in.
4. Run **`CDPLAYER`** with an **audio** CD in the drive.

What to tell me, whichever way it goes:

- What did `UIDE` print? If it says `no drives found` the drive may be on a
  controller it does not handle — that is useful data, and `/UX` (add it to the
  `UIDE.SYS` line in `C:\CONFIG.SYS`) turns UltraDMA off and falls back to PIO,
  which is worth trying before giving up.
- Did `SHSUCDX` assign `D:`, and does `DIR D:` list a data disc?
- Does `CASTID` show `Boot profile CDROM` on this profile?
- In `CDPLAYER`: does it find the drive, read the **track list**, and report
  sensible track times? Does play/pause/skip work, and does audio actually come
  out of the drive's own jack or the sound card?
- If it fails, the exact screen text matters more than a description — the tool
  is written to say *why* it failed rather than just refusing.

---

## 86Box: the tier that can settle almost everything left

A cycle-accurate 86Box machine configured to match the real 386SX turned
out to be the most productive test rig in the project, and it can reach
things QEMU cannot. If a 386SX machine profile is already set up, this is
the order worth working through — every item is something no automated test
here can answer.

1. **The CD-ROM profile, with an audio disc.** 86Box emulates ATAPI drives
   *and* CD-DA audio from CUE/BIN images, which is the one thing QEMU has no
   implementation of at all. Boot entry 4, watch `UIDE` and `SHSUCDX`, then
   `DIR D:` on a data image and `CDPLAYER` on an audio one.
2. **The EMS profiles (3, 4, 5).** Under QEMU `Jemm386` cannot place a 64 KB
   page frame anywhere and prints `no suitable page frame found`. Whether a
   386SX memory map has a free window is exactly what an accurate emulator
   can tell us. If it fails there too, the frame address is one line in
   `C:\CONFIG.SYS`.
3. **`MEM /C` on the XMS profile.** The design target is ~628 KB free
   conventional. This is the number the whole profile design exists to
   protect.
4. **Sound Blaster and the PC speaker.** `SETSOUND`, its `T` key for a
   speaker chime, then a minigame with `S` toggling sound. IRQ/DMA behaviour
   is what emulators usually smooth over, but 86Box models it properly.
5. **The mouse**, via `CTMOUSE` on any profile from 2 upward.
6. **`CASTLINK`**, if two machines can be run with their serial ports
   joined — the reference machine reports two COM ports, so it has the
   hardware for it.
7. **`UNDEL`**, against a floppy image with genuinely deleted files.

What 86Box still cannot settle, and the real machine must: the **disk**
benchmark anchor (an emulated IDE image is not a CompactFlash card), a real
CD drive on a real ISA controller, a real sound card's mixer wiring, and
anything about a real VGA card's timing.

---

## Anything else worth a note

- **Memory:** run `MEM /C` on the `XMS` profile and tell me the free
  conventional KB. Under QEMU with 4 MB it reports **622 KB free / 621 KB
  largest executable**; the design target is ~628 KB. Real chipset UMB space is
  what decides it, so your number is the real one.
- **The EMS profiles (3, 4, 5).** Under QEMU, `Jemm386` cannot place a 64 KB
  page frame at all and prints `no suitable page frame found, EMS functions
  limited`. That is an artefact of QEMU's ISA memory map — a real 386SX should
  have a free window. If your machine says the same thing, tell me: the frame
  address in `C:\CONFIG.SYS` is one line to change.
- **`CASTMARK`:** the index is now anchored on a *measured* reference 386SX
  (4 MB, 387 fitted, CLEAN profile: 38 kOps/s, 136 kFLOP/s, 6656 KB/s,
  20.1 screens/s, 530 KB/s), so that machine scores exactly 100 and
  everything else is a real multiple of it. The one anchor still worth
  re-measuring on metal is **disk read**, because an emulated IDE image is
  not a real drive.
- **Sound:** `SETSOUND` has a `T` key that beeps a short rising chime through
  the PC speaker. The minigames also use it; `S` toggles sound in every one.
- **Anything that looks wrong on screen.** Two layout bugs were already found
  simply by rendering a screen and looking at it; a 386SX with a real VGA card
  may show up more.

Failures are the useful outcome here. A build that boots and does everything
teaches us nothing we did not already believe — and the last screenshot you
sent was worth more than every green emulator run before it.
