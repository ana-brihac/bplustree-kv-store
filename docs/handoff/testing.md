# Testing

This project treats testing as the primary correctness story — there's no
formal spec beyond `validate_tree()`'s invariant checks, so the test suite
*is* the definition of "correct." See [build-environment.md](build-environment.md)
for the raw `make` commands.

## Layer 1: unit/integration suite (`make test`)

Each `tests/test_*.c` compiles to its own binary in `tests/bin/`, statically
linked against the `src/*.c` it needs (see the `Makefile`'s per-target and
pattern rules). `make test` runs every binary in `$(TESTS)`, captures
stdout, and diffs it against a golden file in `tests/expected/*.expected`
(`--strip-trailing-cr -B`, so line-ending/blank-line differences between
POSIX and Windows don't spuriously fail). A non-zero exit code (including
an ASan/UBSan abort) is reported separately as `FAIL (crash / ASAN error)`
and its stderr is printed inline.

Notable tests beyond the obvious per-operation ones
(`test_insert_basic`, `test_delete`, `test_split_leaf`, ...):

- **`test_validate`** — randomized fuzzing, up to 1,000,000 insert/delete
  ops, calling `validate_tree()` after *every single operation*. This is
  the main defense against subtle split/merge/borrow bugs.
- **`test_recovery`** — kills a process mid-workload (in-process
  simulation, not a real SIGKILL) and verifies data inserted before the
  "crash" survives a reopen.
- **`test_checkpoint`** — verifies data survives a crash immediately
  *after* a checkpoint, i.e. when the WAL has already been cleared and the
  main DB file must be complete on its own.
- **`test_reopen`** — verifies `root_id` (via the `MetaPage`) persists
  correctly across a clean close/reopen cycle.
- **`test_merge_guard`** — verifies `merge_with_sibling` refuses a merge
  that would exceed `MAX_KEYS`, and that a valid merge still succeeds.
- **`test_serialize`** — round-trips `Node` through `serialize_node`/
  `deserialize_node`.

When adding a new test: create `tests/test_x.c`, add `test_x` to `TESTS`
in the `Makefile`, run it once to see its actual output, and save that as
`tests/expected/test_x.expected` (only after confirming the output is
actually correct, not just "whatever it printed").

## Layer 2: kill-9 chaos tests (`make chaos`, `make stress`)

Real crash tests, not simulated: `tests/chaos_test.py` /
`tests/final_stress_test.py` `Popen` a workload binary
(`run_workload` / `stress_workload`), let it run for a random
10-200ms, `SIGKILL` it (`p.kill()`), then run a verifier binary
(`verify_workload` / `verify_stress`) that reopens the DB and checks every
previously-committed key survived. `chaos` does 50 cycles on a smaller
workload, `stress` does 100 on a larger one. **The DB is wiped clean
before the run starts** — every cycle begins from a fresh empty database.
This is why these tests, despite being real SIGKILLs, did not catch the
WAL-hole bug (see below) — that bug only manifested from the *second*
recovery onward against the same file.

Requires Python 3 (`subprocess`, `random`, `time` — stdlib only).

## Layer 3: persistent crash-cycle test (`make crash-cycle`)

`tests/crash_cycle.c` + `tests/crash_cycle_test.py` — the test that
actually caught the 2026-08-23 WAL-hole data-loss bug (see
[wal-recovery.md](wal-recovery.md) and
[known-issues.md](known-issues.md)). The key difference from chaos/stress:
**the database is created once and survives every cycle** — it is never
wiped between kills. From the second cycle onward, `tree_open()` must
recover a non-empty WAL left behind by the *previous* kill, which is
exactly the `wal_recover()` + `wal_clear()` code path that chaos/stress
never exercises (they always recover from a WAL that was written by a
process starting from a clean file). After every kill, the reopened tree
must contain every key the workload had acknowledged (tracked in a
tracker file, swapped in atomically via `atomic_replace` — see
[platform-portability.md](platform-portability.md)), and `validate_tree()`
must still pass.

```bash
python3 tests/crash_cycle_test.py [iterations] [seed]   # defaults: 20 iterations, a fixed seed
```

If you're changing anything in `wal_recover`, `wal_clear`, or
`wal_checkpoint`, **run this specific test**, not just `make test` — it's
the only one that exercises repeated recovery against the same file.

## Layer 4: in-process invariant stress (`make stress-invariants`)

`tests/test_stress_invariants.c`, run as
`test_stress_invariants 10000` by the `stress-invariants` target — random
insert/delete ops in a single process (no crashes involved), validating
the tree after every op. Cheaper and faster than `test_validate`'s
million-op run; useful as a quick sanity check while iterating on
tree-structure code specifically.

## Practical guidance

- Changing `bplustree.c` (split/merge/borrow logic): run `make test`
  (`test_validate` is the important one) plus `make stress-invariants`.
- Changing `wal.c` or the dirty/flush paths in `buffer_pool.c`: run
  `make test`, `make crash-cycle`, and ideally `make chaos`/`make stress`
  too — durability bugs have historically hidden in the
  recover-from-a-recovery path that only `crash-cycle` exercises.
- Changing `page_manager.c` or anything file-I/O-related (`open()` flags,
  offsets): also check [platform-portability.md](platform-portability.md)
  — Windows CRLF corruption previously showed up as *unrelated-looking*
  `test_checkpoint`/`test_reopen` failures, not an obvious I/O error.
