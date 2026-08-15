# tests/

Test assets, logs, and the recorded results of the compatibility test matrix.

The matrix itself — 60+ planned cases across boot, install, filesystem, memory,
mouse, sound, CD-ROM, game-launch, batch, Windows 3.x, real-hardware, and
emulator-parity categories — is defined in
[`../docs/TESTING.md`](../docs/TESTING.md).

Layout as testing begins:

```
tests/
├── logs/           dated run logs (emulator + real hardware)
├── mem/            MEM /C captures per profile, per machine
└── results/        filled-in copies of the TESTING.md matrix per release
```

The regression gate (the minimal subset that must pass before any release) is
listed at the end of `docs/TESTING.md`.
