# Handoff: B+Tree Storage Engine

## What this project is

A persistent, embedded key-value store written in C: an on-disk B+Tree with
page-based storage, an LRU buffer pool, and a redo-only Write-Ahead Log for
crash recovery. It's a storage *engine*, not a service — there's no server,
no network layer, no external dependencies beyond libc. Think "the storage
layer of SQLite/LMDB," in miniature and for learning/portfolio purposes.

Keys are `int`, values are `void *` (stored as `int64_t` on disk — see
[on-disk format](docs/handoff/on-disk-format.md)). There is no SQL, no
schema, no transactions API beyond the implicit one described in
[WAL & recovery](docs/handoff/wal-recovery.md).

## Architecture at a glance

Four layers, each in its own `src/*.c`/`.h` pair, wired together by `Tree`
(`src/bplustree.c`):

```
Tree (bplustree.c)          <- public API: tree_open/close, insert, search,
  |                            deleteNode, range_search, validate_tree
  v
BufferPool (buffer_pool.c)  <- in-memory cache of pages, LRU eviction,
  |  |                          pin/unpin contract, dirty tracking
  |  v
  |  WAL (wal.c)            <- redo log; every dirty page is WAL-appended
  |                            before it's allowed to be evicted/flushed
  v
PageManager (page_manager.c) <- raw fixed-size (4096B) page I/O on the
                                 single backing file
```

`serialize.c` converts between the in-memory `Node` struct and the raw
`PAGE_SIZE` byte buffers the buffer pool and page manager pass around.

Full detail: [Architecture](docs/handoff/architecture.md).

## Getting started

Requires `gcc` and (for the crash-test harnesses) `python3`. On Windows,
use a mingw-w64 `gcc` — see
[Platform portability](docs/handoff/platform-portability.md) for why plain
MSVC won't work here.

```bash
make test              # build + run the unit/integration suite (golden-file diffed)
make chaos              # 50 kill-9 crash-recovery cycles
make stress              # 100 kill-9 cycles, larger workload
make crash-cycle         # repeated kill-9 against a persistent (never-wiped) DB
make stress-invariants   # 10k random in-process ops, tree validated after every one
```

Details and what each target actually does:
[Build & environment](docs/handoff/build-environment.md) and
[Testing](docs/handoff/testing.md).

## Where to go next

| Topic | File |
|---|---|
| Layers, data flow, pin/unpin contract, invariants | [architecture.md](docs/handoff/architecture.md) |
| Page layout, `Node`/`MetaPage` format, why it's not portable | [on-disk-format.md](docs/handoff/on-disk-format.md) |
| WAL record format, transaction grouping, checkpointing, recovery algorithm | [wal-recovery.md](docs/handoff/wal-recovery.md) |
| Toolchain, `Makefile` targets, build flags | [build-environment.md](docs/handoff/build-environment.md) |
| Windows/mingw shims, `O_BINARY`, why they exist | [platform-portability.md](docs/handoff/platform-portability.md) |
| Test suite structure, chaos/stress harnesses, adding a test | [testing.md](docs/handoff/testing.md) |
| Naming, error-handling style, code layout conventions | [conventions.md](docs/handoff/conventions.md) |
| Recently fixed bugs and what to still watch out for | [known-issues.md](docs/handoff/known-issues.md) |

## Current status

Core B+Tree, buffer pool, page manager, and WAL are complete and covered by
unit tests plus SIGKILL-based chaos tests. The most recent commit
(`36c70de`) fixed a real data-loss bug in WAL recovery and a set of Windows
build/portability gaps — see
[known-issues.md](docs/handoff/known-issues.md) before touching
`wal.c`/`buffer_pool.c` durability paths.
