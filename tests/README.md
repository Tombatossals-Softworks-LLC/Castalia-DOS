# tests/

Test assets, logs, and the recorded results of the compatibility test matrix.

The matrix itself — 60+ planned cases across boot, install, filesystem, memory,
mouse, sound, CD-ROM, game-launch, batch, Windows 3.x, real-hardware, and
emulator-parity categories — is defined in
[`../docs/TESTING.md`](../docs/TESTING.md).

Layout:

```
tests/
├── unit/           host unit tests (scripts/test-unit.sh)
├── dos/            programs run inside DOS by the CI e2e scripts
└── results/        test runs, one file each: <date>-<tester>-<env>.md
```

`logs/` (raw run logs) and `mem/` (`MEM /C` captures per profile and machine)
join them when the first such capture is recorded.

The regression gate (the minimal subset that must pass before any release) is
listed at the end of `docs/TESTING.md`.
