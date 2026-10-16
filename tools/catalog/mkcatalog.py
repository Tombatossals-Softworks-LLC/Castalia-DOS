#!/usr/bin/env python3
"""Build and check CATALOG.TXT, the index CATALOG.EXE downloads.

    mkcatalog.py build MANIFEST.json ZIPDIR BASEURL -o CATALOG.TXT
    mkcatalog.py check CATALOG.TXT [ZIPDIR]

The manifest is a JSON list, one object per game:

    {"id": "SNAKE", "name": "Castalia Snake", "year": 2026,
     "genre": "Arcade", "cpu": 386, "exe": "SNAKE.EXE",
     "profile": "CLEAN", "sound": "SPKR", "mouse": false,
     "license": "MIT", "notes": "...", "zip": "SNAKE.ZIP",
     "source": "https://... where the file and its licence come from"}

Size, CRC-32 and the unpacked size are measured from the ZIP, never
typed: a figure copied by hand is the one that is wrong.  'source' is
not published in the index; it is required so that every file on the
server can be traced back to its permission to be there.

Every rule CATIDX.C applies on the DOS side is applied here first, plus
the ones only the host can check: that the program is at the root of
the archive, and that every name in it fits DOS's 8.3 (UNZIP on DOS
would shorten a long name and the game would not find its own files).

Python 3 standard library only.
"""

import argparse
import json
import os
import re
import sys
import zipfile
import zlib

MAGIC = "CASTALIA-CATALOG 1"
PROFILES = ("CLEAN", "XMS", "EMS", "CDROM")
SOUNDS = ("NONE", "SPKR", "ADLIB", "SB", "SBPRO", "SB16")
CPUS = (86, 286, 386, 486, 586)
SIZE_MAX = 64 * 1024 * 1024
DEVICES = {"CON", "PRN", "AUX", "NUL", "CLOCK$"} | {
    f"{d}{n}" for d in ("COM", "LPT") for n in range(1, 10)}
NAME83 = re.compile(r"^[A-Z0-9_-]{1,8}$")
URL = re.compile(r"^http://[A-Za-z0-9.-]+(:[0-9]{1,5})?/[A-Za-z0-9\-._~/%+=&?:@,]*$")
LIMITS = {"name": 40, "genre": 14, "license": 24, "notes": 76}
CLUSTER = 4096          # round unpacked files up to a 4 KB FAT16 cluster


class CatalogError(Exception):
    pass


def plain(text, field, may_be_empty):
    if text == "" and not may_be_empty:
        raise CatalogError(f"{field} is empty")
    if any(not (0x20 <= ord(c) <= 0x7E) for c in text) or "|" in text:
        raise CatalogError(f"{field} must be printable ASCII without '|': {text!r}")
    if len(text) > LIMITS[field]:
        raise CatalogError(f"{field} is longer than {LIMITS[field]}: {text!r}")


def name83(base):
    return bool(NAME83.match(base)) and base not in DEVICES


def dos_path_ok(path):
    """Every component of a member name fits 8.3 and none climbs out."""
    parts = [p for p in path.replace("\\", "/").split("/") if p]
    if not parts or path.startswith("/") or ":" in path:
        return False
    for part in parts:
        if part in (".", ".."):
            return False
        base, _, ext = part.upper().partition(".")
        if not name83(base) or len(ext) > 3 or "." in ext:
            return False
        if ext and not re.match(r"^[A-Z0-9_-]+$", ext):
            return False
    return True


def check_entry(e):
    """The DOS parser's rules, for one manifest entry or index line."""
    if not name83(e["id"]):
        raise CatalogError(f"bad id {e['id']!r} (A-Z 0-9 _ -, 1-8, not a device)")
    for field, empty in (("name", False), ("genre", True), ("license", False), ("notes", True)):
        plain(e[field], field, empty)
    year = int(e["year"])
    if year != 0 and not 1970 <= year <= 2099:
        raise CatalogError(f"{e['id']}: year {year} out of range")
    if int(e["cpu"]) not in CPUS:
        raise CatalogError(f"{e['id']}: cpu must be one of {CPUS}")
    base, _, ext = e["exe"].partition(".")
    if not name83(base) or ext not in ("EXE", "COM", "BAT"):
        raise CatalogError(f"{e['id']}: bad exe {e['exe']!r} (8.3, .EXE/.COM/.BAT, upper case)")
    if e["profile"] not in PROFILES:
        raise CatalogError(f"{e['id']}: profile must be one of {PROFILES}")
    if e["sound"] not in SOUNDS:
        raise CatalogError(f"{e['id']}: sound must be one of {SOUNDS}")
    if not 0 < int(e["size"]) <= SIZE_MAX:
        raise CatalogError(f"{e['id']}: size {e['size']} out of range")
    if not 0 < int(e["disk_kb"]) <= 2097152:
        raise CatalogError(f"{e['id']}: disk_kb out of range")
    if len(e["url"]) > 127 or not URL.match(e["url"]):
        raise CatalogError(f"{e['id']}: url must be plain http://host/path, 127 chars max")


def measure_zip(path, exe):
    """Size, CRC-32 and unpacked KB of a ZIP, after checking its names."""
    with open(path, "rb") as fp:
        data = fp.read()
    with zipfile.ZipFile(path) as zf:
        bad = zf.testzip()
        if bad is not None:
            raise CatalogError(f"{path}: corrupt member {bad}")
        names = [i for i in zf.infolist() if not i.is_dir()]
        for info in zf.infolist():
            if not dos_path_ok(info.filename.rstrip("/")):
                raise CatalogError(f"{path}: {info.filename!r} is not a DOS 8.3 path")
        roots = {i.filename.upper() for i in names if "/" not in i.filename}
        if exe not in roots:
            raise CatalogError(f"{path}: {exe} is not at the root of the archive")
        disk = sum(-(-i.file_size // CLUSTER) * CLUSTER for i in names)
    return len(data), zlib.crc32(data) & 0xFFFFFFFF, max(1, -(-disk // 1024))


def build(manifest_path, zipdir, base_url, out_path):
    with open(manifest_path, encoding="utf-8") as fp:
        manifest = json.load(fp)
    if not base_url.endswith("/"):
        base_url += "/"
    seen, lines = set(), [MAGIC, "; Generated by tools/catalog/mkcatalog.py - do not edit by hand."]
    for g in manifest:
        for key in ("id", "name", "exe", "profile", "sound", "license", "zip", "source"):
            if not str(g.get(key, "")).strip():
                raise CatalogError(f"manifest entry {g.get('id', '?')}: '{key}' is required")
        if g["id"] in seen:
            raise CatalogError(f"duplicate id {g['id']}")
        seen.add(g["id"])
        size, crc, disk_kb = measure_zip(os.path.join(zipdir, g["zip"]), g["exe"])
        e = {
            "id": g["id"], "name": g["name"], "year": int(g.get("year", 0)),
            "genre": g.get("genre", ""), "cpu": int(g.get("cpu", 386)),
            "size": size, "disk_kb": disk_kb, "crc": crc, "exe": g["exe"],
            "profile": g["profile"], "sound": g["sound"],
            "mouse": "yes" if g.get("mouse") else "no",
            "license": g["license"], "url": base_url + g["zip"],
            "notes": g.get("notes", ""),
        }
        check_entry(e)
        lines.append("|".join([
            e["id"], e["name"], str(e["year"]), e["genre"], str(e["cpu"]),
            str(e["size"]), str(e["disk_kb"]), f"{e['crc']:08X}", e["exe"],
            e["profile"], e["sound"], e["mouse"], e["license"], e["url"],
            e["notes"]]))
    for line in lines:
        if len(line) > 510:
            raise CatalogError(f"line longer than 510 characters: {line[:40]}...")
    with open(out_path, "wb") as fp:
        fp.write(("\r\n".join(lines) + "\r\n").encode("ascii"))
    return len(lines) - 2


def check(index_path, zipdir=None):
    with open(index_path, "rb") as fp:
        text = fp.read().decode("ascii")
    rows = [l for l in text.replace("\r\n", "\n").split("\n")]
    first = next((l for l in rows if l.strip()), "")
    if first.strip() != MAGIC:
        raise CatalogError(f"first line is not {MAGIC!r}")
    n = 0
    for line in rows[rows.index(first) + 1:]:
        if not line.strip() or line.strip().startswith(";"):
            continue
        f = [x.strip() for x in line.split("|")]
        if len(f) != 15:
            raise CatalogError(f"{len(f)} fields, not 15: {line[:40]}")
        e = dict(zip(("id", "name", "year", "genre", "cpu", "size", "disk_kb",
                      "crc", "exe", "profile", "sound", "mouse", "license",
                      "url", "notes"), f))
        check_entry(e)
        if not re.match(r"^[0-9A-Fa-f]{8}$", e["crc"]):
            raise CatalogError(f"{e['id']}: bad CRC-32")
        if e["mouse"].lower() not in ("yes", "no"):
            raise CatalogError(f"{e['id']}: mouse must be yes or no")
        if zipdir:
            zpath = os.path.join(zipdir, e["url"].rsplit("/", 1)[1])
            size, crc, _ = measure_zip(zpath, e["exe"])
            if size != int(e["size"]) or crc != int(e["crc"], 16):
                raise CatalogError(f"{e['id']}: {zpath} does not match the index")
        n += 1
    return n


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build")
    b.add_argument("manifest")
    b.add_argument("zipdir")
    b.add_argument("base_url")
    b.add_argument("-o", "--output", default="CATALOG.TXT")
    c = sub.add_parser("check")
    c.add_argument("index")
    c.add_argument("zipdir", nargs="?")
    args = ap.parse_args(argv)
    try:
        if args.cmd == "build":
            n = build(args.manifest, args.zipdir, args.base_url, args.output)
            print(f"{args.output}: {n} game(s)")
        else:
            n = check(args.index, args.zipdir)
            print(f"{args.index}: {n} game(s), all valid")
    except (CatalogError, KeyError, ValueError, OSError, zipfile.BadZipFile) as exc:
        print(f"ERR  {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
