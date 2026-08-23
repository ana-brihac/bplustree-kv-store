# Crash recovery / durability (WAL)

This is the most delicate part of the codebase — a real data-loss bug here
was found and fixed on 2026-08-23 (commit `36c70de`). Read
[known-issues.md](known-issues.md) alongside this file before changing
anything in `src/wal.c` or the dirty/checkpoint paths in
`src/buffer_pool.c`.

## Model: redo-only, whole-page, checksummed

There is no undo log — only redo. Every WAL record is a full
`PAGE_SIZE` snapshot of a page *after* modification:

```c
typedef struct {
    page_id_t page_id;
    int seq_num;
    uint32_t checksum;   // over page_data, detects torn writes
    int tx_records;      // how many records belong to this transaction
    char page_data[PAGE_SIZE];
} WALRecord;
```
(`src/wal.h:18-24`)

`checksum` is a simple djb2-like rolling hash (`calculate_checksum`,
`src/wal.c:21-28`) — good enough to detect a torn/partial write from a
crash mid-`write()`, not a cryptographic integrity guarantee.

## Transaction grouping

A "transaction" here is implicit: **all pages dirtied during one
`insert()` or `deleteNode()` call**, which may be several pages if a split
or a cascading merge touched multiple nodes. The grouping works like this:

1. As the tree op recurses, every `bp_unpin(..., mark_dirty=true)` calls
   `wal_append(wal, page_id, data)` (`src/buffer_pool.c:151-153`), which
   buffers the record **in memory** (`wal->buffer`, sized for 64 records —
   the hard cap on pages touched by one call, see
   [known-issues.md](known-issues.md)). If the same page is dirtied twice
   in one call, `wal_append` overwrites the buffered record in place
   rather than appending a duplicate (`src/wal.c:65-74`) — a small
   in-memory write-coalescing optimization.
2. At the end of the top-level `insert`/`deleteNode` call,
   `wal_fsync(tree->wal)` runs. It stamps `tx_records = num_buffered` on
   **every** buffered record (`src/wal.c:92-94`) — this is how a reader
   later knows how many records belong to the same atomic unit — then
   writes them all in a single `write()` call and calls `fsync()`
   (`src/wal.c:95-107`).

So "the transaction" is only durable once `wal_fsync` returns — before
that, a crash loses the whole in-progress operation, which is correct
redo-log semantics (the main DB file was never touched either).

## Recovery algorithm (`wal_recover`, `src/wal.c:120-163`)

Runs on every `tree_open()` (see below), scanning the WAL file from byte 0:

1. Read one `WALRecord` at a time.
2. Verify its checksum. Mismatch → torn write → **stop recovery here**
   (everything after a torn record is by definition never confirmed
   written).
3. Verify `record.seq_num == wal->next_seq_num` (a strictly increasing
   counter starting at 0 for this scan). Any gap → stop (same reasoning:
   an out-of-order or missing seq_num means the log tail is unreliable).
4. Buffer records into a local 64-entry array until `tx_records` records
   for the current transaction have all been read, matching the
   `tx_records` value stamped on the first record of the batch. **Only
   then** are all of them applied via `pm_write_page` directly to the main
   DB file (bypassing the buffer pool entirely). This makes each
   transaction atomic on replay: a transaction with only some of its
   records present (crash mid-`wal_fsync`'s `write()`) is never partially
   applied.
5. A `tx_records` mismatch mid-transaction also stops recovery
   (`src/wal.c:144-147`).

## Checkpointing (`wal_checkpoint`, `src/wal.c:184-211`)

Bounds WAL growth: `Tree.insert`/`deleteNode` trigger it automatically
once `wal->next_seq_num >= 100` (`src/bplustree.c:1257-1259`,
`1289-1291`). Order matters and is deliberately conservative:

1. `wal_fsync` — flush any still-buffered records to the WAL file first.
2. `bp_flush_all(bp)` — write every dirty buffer-pool frame to the main DB
   file. **If any page fails to flush, checkpoint aborts and returns
   `false` without truncating the WAL** — the WAL is still the only
   durable copy of that write, so truncating here would lose it. (This
   `bool` return was added in the 2026-08-23 fix; previously
   `bp_flush_all` was `void` and a failed flush was silently ignored,
   letting the WAL get truncated anyway — see
   [known-issues.md](known-issues.md).)
3. `fsync(bp->pm->fd)` — make sure the OS has actually persisted those
   page writes before touching the log.
4. `ftruncate` the WAL to 0 and `lseek` back to offset 0, reset
   `next_seq_num = 0`.

## `wal_clear` vs `wal_checkpoint` — don't confuse them

`wal_clear` (`src/wal.c:165-182`) is the lighter-weight sibling, called
right after `wal_recover` on every `tree_open()` — it assumes recovery
already got everything into the main DB file via direct `pm_write_page`
calls, so it just needs to empty the log:

```c
void wal_clear(WAL *wal) {
    ftruncate(wal->fd, 0);
    lseek(wal->fd, 0, SEEK_SET);   // <-- the 2026-08-23 fix; see below
    wal->next_seq_num = 0;
}
```

It does **not** call `fsync` on the main DB file and does **not** go
through the buffer pool — it trusts that `wal_recover` already wrote
everything durably via direct page writes.

## What `tree_open()` does with all this (`src/bplustree.c:18-90`)

```c
newTree->wal = wal_open(wal_filename);
if (!newTree->wal) { /* fail loudly, clean up, return NULL */ }
newTree->bp = bp_create(newTree->pm, newTree->wal);
...
wal_recover(newTree->wal, newTree->bp);   // always — even on a "fresh" WAL
wal_clear(newTree->wal);
```

Both of these are unconditional as of the 2026-08-23 fix. Previously the
code only recovered `if (wal->next_seq_num > 0)`, where `next_seq_num` is
`wal_open`'s guess based on `file_size / sizeof(WALRecord)`
(`src/wal.c:40-45`) — a guess that comes out to `0` for a WAL truncated by
a torn write below one full record's size, which incorrectly skipped
recovery of everything after it. `wal_recover` on a genuinely empty WAL is
a cheap no-op, so calling it unconditionally is free.

## The bug that was just fixed here — read before touching this file

`wal_clear`'s `ftruncate` does **not** move the file's write offset.
Before the fix, the code truncated the file but left the fd's offset
wherever `wal_recover`'s read loop had stopped. The next `wal_fsync` would
then write starting at that stale offset instead of 0, leaving a
**zero-byte hole** at the head of the file. A zeroed `WALRecord` happens to
checksum to `0` and reads back as `seq_num == 0`, so the *next* crash's
`wal_recover` would mistake that hole for one bogus valid transaction,
advance past it, and then reject the real `seq_num == 0` record that
followed as "out of order" — silently discarding every record written
since the previous recovery. It only manifested after **two or more**
crash/recover cycles against the same DB, which is why the in-process test
suite (fresh DB per test) never caught it — it took
`tests/crash_cycle.c` (repeated kill-9 against a *persistent*, never-wiped
DB) to surface it. See [testing.md](testing.md) for that harness and
[known-issues.md](known-issues.md) for the full list of what else changed
in the same fix.

**Takeaway for future changes:** any code that truncates or repositions
the WAL file must also reset the fd's write offset in the same breath.
`ftruncate` alone is not enough.
