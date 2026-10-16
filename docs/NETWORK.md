# Network and the online game catalogue

> Status: **implemented** — `CATALOG.EXE` (`src/catalog/`), the network kit
> (`fetch-payload.sh --with-net`), the index tool (`tools/catalog/mkcatalog.py`)
> and an end-to-end test that installs a game over an emulated NE2000 card
> (`scripts/test-catalog.sh`). Hard-disk distribution only.

Castalia 1.0 drew the line at networking ("post-1.0", [`ARCHITECTURE.md`](ARCHITECTURE.md),
[`COMPATIBILITY.md`](COMPATIBILITY.md)). This is the first thing past that line,
and it is deliberately small: the network exists only while the catalogue is
open. Boot profiles, conventional memory and the games themselves are exactly
as they were.

---

## 1. What the user sees

Main menu → **C  Game Catalogue (online)**, or `CATALOG` at the prompt.

1. The packet driver for the card is loaded and DHCP asks for an address.
2. The catalogue is downloaded and listed: name, year, genre, minimum CPU,
   download and disk size, recommended boot profile, sound, mouse, licence,
   and whether it is already installed.
3. **Enter** on a game: confirm, then it is downloaded, checked, unpacked into
   `C:\GAMES\<ID>` and added to `GAMES.INI`.
4. **Esc**: the driver is unloaded. The game is in the Game Launcher, with its
   boot profile recommendation, like any other.

For scripts and tests:

```
CATALOG /LIST           print the catalogue and exit
CATALOG /INSTALL <ID>   install one game without the UI
CATALOG /URL <url>      use another catalogue than [catalog] url
CATALOG /NONET          the network is already up: load nothing
```

Exit codes: 0 done, 1 usage, 2 network, 3 not a catalogue, 4 install failed.

---

## 2. Getting a 386 onto a network

Anything with a **packet driver** works; the kit carries the common ones.

| Connection | What you need | `[net] driver =` |
|---|---|---|
| ISA Ethernet, NE2000 or clone | the card, a cable to a router | `NE2000.COM 0x60 <irq> <port>`, e.g. `NE2000.COM 0x60 10 0x300` |
| 3Com EtherLink III (3C509) | the card, a cable to a router | `3C509.COM 0x60` |
| Serial port, SLIP | a null-modem cable to a machine that routes SLIP (on Linux: `slattach` + NAT) | `ETHERSL.COM 0x60 4 0x3F8 19200` (COM1) |
| Parallel port, PLIP | a LapLink-style cable to a machine that routes PLIP (Linux `plip` module) | `PLIP.COM 0x60 7 0x378` (LPT1) |
| QEMU | `-device ne2k_isa,iobase=0x300,irq=9` + `-netdev user` | `NE2000.COM 0x60 9 0x300` |
| DOSBox-X | `[ne2000] ne2000=true nicbase=300 nicirq=3 backend=slirp` | `NE2000.COM 0x60 3 0x300` |
| 86Box | an NE2000 in the machine config, SLiRP networking | the IRQ/port you set |

Other cards: drop their packet driver into `C:\CASTALIA\NET` and name it in
`driver =`; Castalia only cares that something answers on the packet interrupt.

The IRQ and port must not collide with the sound card. The usual clash is a
Sound Blaster on IRQ 5 or 7 and an NE2000 jumpered to the same line; IRQ 10 or
3 (with COM2 unused) are the safe choices on most boards.

### Settings (`C:\CASTALIA\CFG\CASTALIA.INI`)

```ini
[net]
driver     = NE2000.COM 0x60 10 0x300
packet_int = 0x60
dhcp       = yes

[catalog]
url        = http://dos.castaliaos.com/catalog/CATALOG.TXT
```

With `dhcp = no`, put `IPADDR`, `NETMASK`, `GATEWAY` and `NAMESERVER` lines in
`C:\CASTALIA\NET\MTCP.CFG` (mTCP's own format); `CATALOG` creates that file with
`PACKETINT` on first use.

---

## 3. How it works

```
CATALOG.EXE
 ├─ packet driver resident already?  yes → use it, leave it loaded at the end
 │                                   no  → spawn [net] driver (it goes TSR above us)
 ├─ spawn DHCP.EXE                    address, gateway, DNS into MTCP.CFG
 ├─ spawn HTGET.EXE  CATALOG.TXT      index (plain HTTP)
 │   … browse …
 ├─ install:
 │   ├─ room in GAMES.INI? (64 games, 16 KB file) — before any download
 │   ├─ room on the disk? (ZIP + unpacked size + 64 KB)
 │   ├─ spawn HTGET.EXE  <ID>.ZIP
 │   ├─ size and CRC-32 against the index — nothing is unpacked otherwise
 │   ├─ spawn UNZIP.EXE -o -qq <ID>.ZIP -d C:\GAMES\<ID>
 │   ├─ the program named in the index is there?
 │   └─ GAMES.INI: copy + new section → swapped in whole (SAFEIO), backup first
 └─ spawn TERMIN.COM 0x60             only if we loaded the driver
```

Design points, each of which a test pins down:

- **Children, not a shell.** Every program runs through `spawnv()`, so its
  exit code comes back intact, and nothing from the index is ever parsed by
  `COMMAND.COM`.
- **Borrowed memory.** The driver is loaded above `CATALOG.EXE` and `TERMIN`
  frees it before `CATALOG` exits, so the block coalesces and the next game
  starts with the memory it had before. A driver loaded by `AUTOEXEC.BAT` is
  the user's and is left alone.
- **One HTGET per second.** HTGET picks its local port as `2048 + rand()`, and
  mTCP seeds `rand()` from `time(NULL)` — whole seconds. Two downloads started
  in the same second reuse the port, the second connection meets the first one
  still closing, and HTGET times out after ten seconds. That is exactly an index
  fetch followed by the game's download, so `CATALOG` never starts two in one
  second (found by the E2E test, `CATALOG.C` `next_second()`).
- **HTTP only.** No 386 can negotiate modern TLS. HTGET does not follow
  redirects, so a server that sends plain-HTTP requests to `https://` shows up
  as "the server redirected the request (to https?)" rather than a mystery.
- **GAMES.INI is only touched last**, and through a whole-file swap. Comments
  and hand edits survive; a full disk leaves the old file in place.

---

## 4. The index: `CATALOG.TXT`

One header line, then one line per game, `|`-separated, CRLF:

```
CASTALIA-CATALOG 1
ID|NAME|YEAR|GENRE|CPU|SIZE|DISKKB|CRC32|EXE|PROFILE|SOUND|MOUSE|LICENSE|URL|NOTES
```

| Field | Rule |
|---|---|
| `ID` | 1–8 of `A-Z 0-9 _ -`, not a DOS device name. Also the folder under `C:\GAMES`. |
| `NAME` | printable ASCII, ≤ 40 |
| `YEAR` | 1970–2099, or 0 |
| `GENRE` | printable ASCII, ≤ 14, may be empty |
| `CPU` | 86, 286, 386, 486 or 586 |
| `SIZE` | bytes of the ZIP, 1 to 64 MB |
| `DISKKB` | unpacked size in KB (4 KB clusters) |
| `CRC32` | 8 hex digits, the ZIP's CRC-32 |
| `EXE` | 8.3, `.EXE`/`.COM`/`.BAT`, at the root of the ZIP |
| `PROFILE` | `CLEAN`, `XMS`, `EMS` or `CDROM` |
| `SOUND` | `NONE`, `SPKR`, `ADLIB`, `SB`, `SBPRO` or `SB16` |
| `MOUSE` | `yes` / `no` |
| `LICENSE` | printable ASCII, ≤ 24 |
| `URL` | `http://host[:port]/path`, ≤ 127, URL characters only |
| `NOTES` | printable ASCII, ≤ 76, may be empty |

Not INI on purpose: the INI module holds one 16 KB file in memory, and a
catalogue should be able to outgrow that. The reader streams a line at a time
through a fixed buffer. `CATALOG` shows the first 128 games; lines it refuses
are counted on screen ("N line(s) skipped"), never half-used.

The rules live in two places that must agree: `src/common/CATIDX.C` (DOS) and
`tools/catalog/mkcatalog.py` (host). `tests/unit/test_catidx.c` exercises the
DOS side, mostly with things it must refuse.

---

## 5. What the checksum is for

The CRC-32 catches a **damaged or truncated** download — the common failure
on a 10 Mbit card and an old cable. It does **not** prove where a file came
from: the index travels over the same plain HTTP as the games, so someone able
to tamper with the traffic could change both.

What limits that today:

- the index is validated field by field, and a ZIP is unpacked only into
  `C:\GAMES\<ID>`; Info-ZIP 6 strips `../` and drive letters from member names;
- nothing in the index is ever run through a shell;
- GAMES.INI is backed up before every change.

What would close it: a **signed index** — the server signs `CATALOG.TXT`,
`CATALOG.EXE` verifies against a public key compiled in, and per-game SHA-256
replaces CRC-32 in the signed file. One Ed25519 verification per catalogue
fetch is the cost; how long that takes in 16-bit C on a 386SX has not been
measured, and should be before choosing. Not implemented yet; it is
the next step if the catalogue carries anything beyond the project's own
curated list.

---

## 6. Publishing a catalogue

```sh
tools/catalog/mkcatalog.py build catalog/manifest.json zips/ \
    http://dos.castaliaos.com/games -o CATALOG.TXT
tools/catalog/mkcatalog.py check CATALOG.TXT zips/
```

`mkcatalog.py` measures size, CRC-32 and unpacked size from each ZIP (never
typed by hand), applies the DOS rules, and refuses a ZIP whose names do not
fit 8.3 or whose program is not at its root. See [`../catalog/README.md`](../catalog/README.md)
for the manifest, and the licensing policy every entry must meet.

**The server must answer plain HTTP without redirecting.** castaliaos.com is
behind Cloudflare with *Always Use HTTPS* on, which turns every `http://`
request into a 301 that HTGET cannot follow. Either:

- a **Configuration Rule** on castaliaos.com that turns *Always Use HTTPS* off
  for `dos.castaliaos.com` (keeps the CDN and its protection), or
- a DNS-only (grey cloud) `dos.castaliaos.com` pointing at the origin, with an
  nginx `server { listen 80; }` serving the catalogue directory and nothing else.

Either way, check from any machine with `curl -sI http://<host>/catalog/CATALOG.TXT`:
the answer must be `200`, not `301`.

---

## 7. Limits

| Limit | Value | Where |
|---|---|---|
| Games shown | 128 | `MAX_ENTRIES`, `CATALOG.C` |
| Games in the launcher | 64 | `MAX_GAMES`, `LAUNCH.C`; checked before downloading |
| `GAMES.INI` size | 16 KB | `INI_MAX_SIZE`; checked before downloading |
| Download | 64 MB | `CAT_SIZE_MAX` |
| Index line | 510 characters | `CAT_LINE_MAX` |
| Protocols | plain HTTP, DHCP or static IP, DNS from DHCP | mTCP |

---

## 8. Testing

`scripts/test-catalog.sh` boots the real floppy image in QEMU with an NE2000
on user-mode networking and a FAT16 hard disk holding an installed Castalia,
serves a test catalogue from the host, and checks from the outside that:

the driver loads, DHCP answers and the catalogue lists; a hostile index line
is skipped; a download with a wrong CRC is refused and not registered; a good
game downloads, unpacks (subdirectory included), registers in GAMES.INI with
everything already there kept and a backup made, and runs; and afterwards
HTGET alone fails — the driver is gone. It needs `fetch-payload.sh --with-net`
and the built tools, and finishes in about thirty seconds.
