# Conventions

There's no `.clang-format`, linter config, or style guide in the repo —
these are the conventions as observed in the existing code. Match them
when adding to existing files; for new files, prefer the tab-indented
style described below since it's the more common one.

## Naming

- **Types** (`Node`, `Tree`, `BufferPool`, `PageManager`, `WAL`,
  `MetaPage`, `Frame`, `HashNode`, `WALRecord`) — `PascalCase` structs,
  typedef'd without a `_t` suffix.
- **Scalar typedefs** (`page_id_t`) — `snake_case_t`, POSIX-style.
- **Functions** — `snake_case`, prefixed by owning module: `pm_*`
  (page manager), `bp_*` (buffer pool), `wal_*` (WAL), `tree_*`/bare verbs
  for the tree API (`insert`, `search`, `deleteNode`, `range_search` —
  note `deleteNode` is the one inconsistent camelCase holdout; don't
  "fix" it in an unrelated change, it's an established public API name).
- **Constants** — `UPPER_SNAKE_CASE` `#define`s (`MAX_KEYS`, `PAGE_SIZE`,
  `BUFFER_POOL_SIZE`, `HASH_TABLE_SIZE`, `INVALID_PAGE_ID`,
  `META_MAGIC_NUMBER`).

## Indentation

**Mixed across files — match the file you're editing, don't reformat
wholesale.** `buffer_pool.c`, `page_manager.c`, and most of
`bplustree.c` use tabs; `wal.c` and parts of `bplustree.c` use 4-space
indentation. This is pre-existing inconsistency, not a signal either way
— a drive-by reformat of a whole file is out of scope for an unrelated
change.

## Error handling

No exceptions, no `errno`-style global state beyond what libc itself
sets. The pattern throughout is:

- Pointer-returning functions: `NULL` on failure
  (`pm_open`, `wal_open`, `bp_create`, `create_leaf_node` returns
  `INVALID_PAGE_ID` instead since it returns a `page_id_t`).
- `bool`-returning functions: `true`/`false`
  (`pm_read_page`, `pm_write_page`, `wal_append`, `wal_fsync`,
  `bp_flush_page`, `bp_flush_all`, `validate_tree`).
- Sentinel value for "no such page": `INVALID_PAGE_ID` (`-1`), used
  throughout instead of a separate `bool found` out-parameter.
- Every caller is expected to check these — but not all currently do
  (see [known-issues.md](known-issues.md) for the specific case of
  `wal_append`'s return value being ignored in `bp_unpin`). When adding a
  new call to a function following this convention, check its return
  value; don't add a new exception to the pattern.
- User-facing/diagnostic errors are reported with plain `printf`/`perror`
  to stdout/stderr (e.g. `tree_open`'s failure messages, `wal_recover`'s
  "Torn write detected" message). There is no logging framework, no log
  levels — this is deliberate for a small embedded engine with a
  golden-file-diffed test suite (stdout output *is* the test contract for
  several tests, so don't casually add/remove `printf`s in `bplustree.c`/
  `wal.c` without checking `tests/expected/*.expected`).

## Memory ownership

- `deserialize_node()` and `create_leaf_node`/`create_inner_node`
  allocate a `Node*` the **caller** must `free()`. The buffer pool never
  frees a `Node*` — it only owns the raw `PAGE_SIZE` frame buffers
  (allocated once in `bp_create`, freed once in `bp_destroy`).
- Every `bp_fetch_page` must be paired with a `bp_unpin` — see the
  pin/unpin contract in [architecture.md](architecture.md). This is the
  most important convention in the codebase to preserve; an unpaired
  fetch permanently pins a frame (starves eviction), and an unpaired
  extra unpin can under-count pins and let an in-use frame be evicted.

## Comments

Existing comments mostly explain *why*, especially around subtle
correctness points (see the `wal_clear` comment at `src/wal.c:170-179`
for the style to emulate) — keep new comments in that spirit rather than
narrating *what* the code does line-by-line. Some older comments are
lower-signal inline restatements of the code (e.g. `// checking if we
have memory`) — don't feel obligated to match that style in new code.

## Tests

Test files are more permissive than `src/` — dead variables and looser
style are tolerated there as long as `-Wall -Wextra` stays clean (the
2026-08-23 fix specifically cleaned up warnings left over from an earlier
API refactor across several `tests/*.c` files). Golden-file tests
(`tests/expected/*.expected`) must produce **deterministic** stdout — no
raw pointer values, timestamps, or unseeded randomness in test output.
