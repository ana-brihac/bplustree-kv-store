# Known issues / recent fixes

## Recently fixed (commit `36c70de`, 2026-08-23)

Five real bugs were fixed together; understanding them is useful context
even though they're resolved, because the same *shape* of bug (durability
code that looks right on a single crash but fails on repeated
crash/recover cycles) is the main risk area going forward.

1. **WAL-hole data-loss bug** — `wal_clear()` truncated the WAL but never
   reset the fd's write offset, leaving a zero-byte hole at the head of
   the file after the second recovery cycle, which caused every
   subsequent record to be misparsed as out-of-order and discarded. Fixed
   by adding `lseek(wal->fd, 0, SEEK_SET)` after the `ftruncate`. Full
   mechanism in [wal-recovery.md](wal-recovery.md#the-bug-that-was-just-fixed-here--read-before-touching-this-file).
   Only reproduced via `tests/crash_cycle.c` (repeated kills against a
   persistent DB) — `make chaos`/`make stress` never caught it because
   they wipe the DB before each run.
2. **`wal_checkpoint` swallowed flush failures** — `bp_flush_all` used to
   return `void`; a page that failed to flush to the main DB file was
   silently ignored and the WAL got truncated anyway, discarding the only
   durable copy of that write. Now `bp_flush_all` returns `bool`, and
   `wal_checkpoint` aborts (returns `false`, WAL left intact) if any page
   fails to flush.
3. **`tree_open` silent-no-WAL / skipped-recovery** — previously,
   `wal_open` failing meant the tree opened anyway with no durability and
   no error; and recovery was skipped whenever `wal_open`'s
   file-size-based `next_seq_num` guess came out to `0`, which happens
   for a WAL torn below one record's size — dropping that torn record
   silently instead of scanning past it. Now `tree_open` fails loudly if
   `wal_open` fails, and always calls `wal_recover` regardless of the
   guessed `next_seq_num`.
4. **Windows CRLF corruption** — `page_manager.c`/`wal.c` opened files
   without `O_BINARY`, so Windows' text-mode CRLF translation could
   corrupt raw page/WAL bytes. This was the actual root cause of
   `test_checkpoint`'s byte-count mismatch and `test_reopen`'s invariant
   failure on Windows — see [platform-portability.md](platform-portability.md).
5. **Windows didn't link at all** — no `fsync()`/atomic-rename equivalent
   existed for mingw. Added `src/compat.h`.

## Open / still-worth-watching

These are not regressions from the fix above — they're pre-existing
design points worth knowing about before you extend this area.

- **`wal_append`'s return value is ignored in `bp_unpin`**
  (`src/buffer_pool.c:151-153`): `wal_append(bp->wal, page_id,
  bp->frames[frame_idx].data);` — if this returns `false` (e.g. the
  64-record transaction buffer is full, see next point), the page is
  still marked dirty and the caller proceeds as if it were logged. A
  crash before the next successful flush would lose that write silently.
- **The WAL's in-memory transaction buffer caps at 64 records**
  (`wal->buffer = malloc(64 * sizeof(WALRecord))`, `src/wal.c:48`,
  enforced at `src/wal.c:61-63`). One `insert()`/`deleteNode()` call that
  dirties more than 64 pages in a single operation (a deep cascading
  split/merge — unlikely at `MAX_KEYS = 4` but not impossible with a
  pathological key sequence) would silently fail to log the 65th+ page,
  compounding the issue above. No caller currently checks for this.
- **`META_MAGIC_NUMBER` is written but never verified.** `tree_open`
  writes it when creating a fresh `MetaPage` but nothing checks it when
  opening an existing file — so pointing `tree_open` at a non-database
  file (or a file from an incompatible build) won't be caught by a magic
  number check; it'll just be misread as garbage. See
  [on-disk-format.md](on-disk-format.md) for the broader point that the
  format has no version field at all.
- **On-disk format is a raw struct memcpy, not a defined byte layout** —
  not portable across compilers/architectures/ABIs, and `Node` (80 bytes)
  wastes ~98% of every 4096-byte page. Both are consequences of the
  current design, not bugs to fix opportunistically — see
  [on-disk-format.md](on-disk-format.md) for the full explanation. Worth
  keeping in mind if this project ever needs to move data between
  machines or survive a struct-layout change.
- **No concurrency support.** `Tree`/`BufferPool`/`WAL` assume a single
  thread, single process. This is an intentional scope boundary for an
  embedded engine, not a TODO — see
  [architecture.md](architecture.md#single-threaded-single-writer).
- **Checksums are non-cryptographic** (`calculate_checksum`,
  `src/wal.c:21-28` — a djb2-like rolling hash). Sufficient for detecting
  torn/partial writes from a crash; not designed to catch deliberate
  corruption or bit rot with high probability.

## Not a bug: `MAX_KEYS = 4`

Deliberately tiny so splits/merges/borrows are exercised constantly by
small test inputs. Don't bump it "for realism" without checking whether
any test relies on the exact fan-out this produces (several golden-file
tests encode the exact tree shape after a specific sequence of inserts).
