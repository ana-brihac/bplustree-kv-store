# Architecture

See [HANDOFF.md](../../HANDOFF.md) for the one-paragraph orientation. This
file goes one level deeper: the four layers, how a call moves through them,
and the invariants that hold the tree together.

## The four layers

```
src/bplustree.c   Tree            public API + tree logic (insert/delete/search/split/merge)
src/buffer_pool.c BufferPool      LRU page cache, pin/unpin, dirty tracking, hash-indexed lookup
src/wal.c         WAL             redo-only log, checksummed records, checkpointing
src/page_manager.c PageManager    raw PAGE_SIZE (4096B) read/write on the backing file
src/serialize.c   (free functions) Node <-> raw PAGE_SIZE byte buffer conversion
```

Ownership: `Tree` owns one `WAL*`, one `BufferPool*`, one `PageManager*`
(`src/bplustree.h:32-37`). `BufferPool` holds a pointer to both `PageManager`
and `WAL` (`src/buffer_pool.h:25-31`) because it's the layer that decides
when a page write needs to hit the log (on `bp_unpin(..., mark_dirty=true)`)
versus the backing file (on eviction or `bp_flush_*`).

## The `Node` struct

```c
#define MAX_KEYS 4

typedef struct Node {
    page_id_t page_id;
    page_id_t parent_id;
    int num_keys;
    bool is_leaf;
    int keys[MAX_KEYS];
    union {
        struct { int64_t values[MAX_KEYS]; page_id_t next_id; } leaf;
        struct { page_id_t children[MAX_KEYS + 1]; } inner;
    } data;
} Node;
```
(`src/bplustree.h:12-29`)

- `MAX_KEYS = 4` — deliberately small, makes splits/merges/borrows exercise
  in every test run with tiny datasets. It also means the tree is *very*
  wide/shallow for real data volumes; see
  [known-issues.md](known-issues.md) for the page-utilization implication.
- Leaf nodes carry `values[]` (an `int64_t`, used to store a pointer-sized
  payload — see [on-disk-format.md](on-disk-format.md)) and `next_id`, a
  page id linking leaves left-to-right for range scans.
- Inner nodes carry `children[MAX_KEYS + 1]` page ids only — routing, no
  payload.
- `page_id_t` is `int64_t`, `INVALID_PAGE_ID` is `-1` (`src/page_manager.h`).

Page 0 of every database file is not a `Node` — it's a `MetaPage` holding a
magic number and the current `root_id` (`src/bplustree.h:39-44`). See
[on-disk-format.md](on-disk-format.md) for layout.

## Data flow for one `insert()`/`deleteNode()` call

1. `Tree.insert(tree, key, value)` calls `insert_core(tree->bp, tree->root_id, ...)`
   (`src/bplustree.c:1247-1262`), which recurses down the tree using
   `bp_fetch_page` / `bp_unpin` to read and mutate nodes in place, splitting
   overfull nodes on the way back up.
2. `update_root()` (`src/bplustree.c:1235-1245`) — if the root page id
   changed (a split pushed a new root), it fetches the `MetaPage` (page 0)
   and rewrites `root_id`, unpinning it dirty.
3. `wal_fsync(tree->wal)` — every page touched during step 1-2 was already
   appended to the WAL's in-memory buffer (see below); this call is what
   actually durably persists them (write + `fsync`).
4. If `tree->wal->next_seq_num >= 100`, `wal_checkpoint()` runs — flushes
   dirty buffer-pool pages to the main file and truncates the WAL. This is
   the only place checkpointing is triggered automatically; see
   [wal-recovery.md](wal-recovery.md).

`search()` and `range_search()` are read-only and never touch the WAL.

## The pin/unpin contract

This is the contract every function touching a page must follow
(documented in full at `src/buffer_pool.h:36-53`):

1. `bp_fetch_page(bp, page_id)` returns a pointer to a **raw** `PAGE_SIZE`
   buffer — never a `Node*`. The buffer pool never calls
   `serialize_node`/`deserialize_node` itself.
2. Caller calls `deserialize_node(data)` to get a heap-allocated `Node*`
   (caller must `free()` it — the buffer pool does not track it).
3. To persist a mutation, caller calls `serialize_node(node, data)` to
   write back into the *same* frame buffer, then
   `bp_unpin(bp, page_id, /*mark_dirty=*/true)`.
4. **The returned buffer pointer is only valid while `pin_count > 0`.**
   Once unpinned, the frame can be evicted (and the pointer reused for a
   different page) at any time. Never cache a fetched pointer across an
   unpin.
5. `bp_unpin(..., mark_dirty=true)` is what triggers `wal_append()`
   internally (`src/buffer_pool.c:151-153`) — marking a page dirty *is* the
   act of logging it. There is no separate "log this write" call.

Eviction (`bp_fetch_page`'s miss path, `src/buffer_pool.c:82-97`) only
considers frames with `pin_count == 0`, and additionally skips any frame
still present in the WAL's unflushed buffer
(`wal_contains_page`) — a page that's dirty-but-not-yet-fsynced can't be
evicted, because evicting it would mean losing the only record of the
change if flushing to the main file also failed.

## Rebalancing invariants

- **Split** (`split_leaf`, `split_inner_node`) happens proactively on the
  way down during insert when a node is at `MAX_KEYS`, then
  `insert_into_parent` pushes the median/separator key up.
- **Delete** rebalancing tries borrow-before-merge:
  `try_borrow_from_left_sibling` / `try_borrow_from_right_sibling` first;
  `merge_with_sibling` only if neither sibling has a key to spare.
  `merge_with_sibling` refuses to merge if the combined key count would
  exceed `MAX_KEYS` (tested explicitly by `tests/test_merge_guard.c`).
- **`validate_tree(tree)`** is the ground-truth invariant checker: all
  leaves at the same depth, every node within min/max key bounds, keys
  strictly sorted. Both the fuzz test (`test_validate`) and the stress
  test (`test_stress_invariants`) call it after *every* operation — treat
  a `validate_tree` failure as a real correctness bug, not flaky test
  infrastructure.

## Single-threaded, single-writer

There is no locking anywhere in this codebase. `Tree`/`BufferPool`/`WAL`
are not safe to share across threads or processes. This is a design
choice appropriate for an embedded engine used by one process at a time,
not an oversight to "fix" casually — see
[known-issues.md](known-issues.md) if you're considering adding
concurrency.
