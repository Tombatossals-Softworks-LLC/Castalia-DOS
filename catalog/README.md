# catalog/ — what the online game catalogue offers

`manifest.json` here is the source of the published `CATALOG.TXT`
([`../docs/NETWORK.md`](../docs/NETWORK.md) §4). It starts empty on purpose:
every entry is a decision about redistributing somebody's work, and is made
one at a time against the policy below.

## Adding a game

1. Put the game's ZIP in a working directory. Every name inside must fit DOS
   8.3, and the program must be at the root of the archive (`mkcatalog.py`
   refuses anything else). Repack if needed: what DOS cannot name, UNZIP would
   shorten, and the game would not find its own files.
2. Add an entry to `manifest.json`:

   ```json
   {"id": "KEEN1", "name": "Commander Keen 1: Marooned on Mars",
    "year": 1990, "genre": "Platform", "cpu": 286, "exe": "KEEN1.EXE",
    "profile": "CLEAN", "sound": "SPKR", "mouse": false,
    "license": "Shareware", "notes": "Episode one of three, as distributed.",
    "zip": "KEEN1.ZIP",
    "source": "<where this exact file and its permission to redistribute come from>"}
   ```

   (An illustration of the shape, not an approved entry.)
3. Build and check:

   ```sh
   tools/catalog/mkcatalog.py build catalog/manifest.json zips/ \
       http://dos.castaliaos.com/games -o CATALOG.TXT
   tools/catalog/mkcatalog.py check CATALOG.TXT zips/
   ```

4. Publish `CATALOG.TXT` at `/catalog/` and the ZIPs at `/games/` on a host
   that answers **plain HTTP without redirecting** (NETWORK.md §6).
5. Install it once through `CATALOG.EXE` — in QEMU at least
   (`scripts/test-catalog.sh` shows how) — and run it from the launcher.

## Licensing policy

A file is published only if its licence **explicitly allows anyone to
redistribute it**, and the `source` field records where that permission is
stated. In practice:

- **Yes:** games released as freeware or open source by their rights holder;
  shareware episodes whose licence permits free redistribution of the
  unmodified archive (read each one: some forbid inclusion in collections, some
  forbid charging, some require the original archive untouched); Castalia's
  own work.
- **No:** "abandonware". A game nobody sells any more is still somebody's.
  Registered or full versions of shareware. Anything repacked from a CD whose
  licence covers only the original medium. Anything where the permission
  cannot be shown.

Shareware archives are often redistributable only **unmodified**. If one has
to be repacked to fit 8.3 or to put the program at the root, check that the
licence allows it, or leave the game out.

The `license` field is what the user sees; keep it honest and short
(`Freeware`, `Shareware`, `GPL-2.0`, `MIT`).
