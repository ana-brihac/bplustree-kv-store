# Build & environment

## Toolchain

- **Compiler:** `gcc` (the `Makefile` hardcodes `CC = gcc`). On Windows
  this must be a mingw-w64 `gcc`, not MSVC — see
  [platform-portability.md](platform-portability.md).
- **Flags:** `-Wall -Wextra -g -fsanitize=address,undefined` for
  *everything*, including the test binaries. ASan/UBSan are always on;
  there is no separate "release" build config. Any new code should build
  warning-clean under `-Wall -Wextra` — the 2026-08-23 fix specifically
  cleaned up ~80 dead-variable warnings and a format-string bug to make
  that true again.
- **No package manager, no external dependencies.** Everything is libc +
  a handful of POSIX/Windows headers (`fcntl.h`, `unistd.h`, `io.h`,
  `windows.h`). The only non-C dependency is **Python 3**, used solely by
  the chaos/stress/crash-cycle test harness scripts (they launch, time,
  and `kill -9` the C workload binaries).

## Source layout

```
src/            engine source (bplustree, buffer_pool, page_manager, wal, serialize, compat)
tests/          test sources, plus generated tests/bin, tests/results (gitignored)
tests/expected/ golden stdout files the unit suite diffs against
```

## Makefile targets

| Target | What it does |
|---|---|
| `make` / `make all` | Builds every test binary into `tests/bin/` |
| `make test` | Builds, then runs each unit/integration test, diffing its stdout against `tests/expected/*.expected` (see [testing.md](testing.md)) |
| `make chaos` | Builds `run_workload`/`verify_workload`, then runs `tests/chaos_test.py` — 50 kill-9 cycles |
| `make stress` | Builds `stress_workload`/`verify_stress`, then runs `tests/final_stress_test.py` — 100 kill-9 cycles |
| `make crash-cycle` | Builds `crash_cycle`, then runs `tests/crash_cycle_test.py` — repeated kill-9 against a DB that's **never wiped** between cycles (exercises the recovery-of-a-recovery path) |
| `make stress-invariants` | Builds and runs `test_stress_invariants 10000` — in-process random ops, `validate_tree` after every one |
| `make clean` | Removes built binaries and `tests/results/*.actual` / `*.err` |

Each test binary is linked directly against the engine sources it needs
(see the per-target rules and the catch-all pattern rule
`$(BIN_DIR)/%: tests/%.c $(SRC)` in the `Makefile`) — there's no shared
`.a`/`.so`, every test binary is a fresh static link of `src/*.c` plus one
`tests/*.c`.

## Everyday commands

```bash
make test              # fast correctness check — run this before every commit
make chaos              # slower; run before anything touching wal.c/buffer_pool.c
make crash-cycle         # slower still; the specific test that caught the WAL-hole bug
```

## Generated / ignored files

`.gitignore` excludes build output (`*.o`, `tests/bin/*`,
`tests/results/*`), and database/WAL artifacts left over from manual runs
(`*.db`, `*.wal`, `*.log`, `stress_tracker.txt`, `chaos_tracker.txt`,
`crash_cycle_tracker.txt`/`.tmp`). If you run binaries manually outside
the test harness, clean up files matching those patterns before
committing — a stray `.db`/`.log` in a `git status` is a signal to check
what's in it before adding, not something to `git add -A` past.
