# On-disk format & persistence

See [architecture.md](architecture.md) for how the layers that produce
this format fit together.

## File layout

The main database file is a flat sequence of fixed `PAGE_SIZE = 4096`-byte
pages (`src/page_manager.h:7`), addressed by `page_id_t` (an `int64_t`,
0-indexed):

```
page 0        MetaPage   { magic_number, root_id }   (padded to 4096B)
page 1..N     Node       (leaf or inner, tagged by is_leaf)
```

There is a separate WAL file (`*.wal` by convention, see
[wal-recovery.md](wal-recovery.md)) — it is a different format entirely,
not a sequence of pages.

## Page 0: `MetaPage`

```c
#define META_MAGIC_NUMBER 0x42504C53 // "BPLS"
typedef struct MetaPage {
    uint32_t magic_number;
    page_id_t root_id;
} MetaPage;
```
(`src/bplustree.h:39-44`)

Created once, when `tree_open()` sees `num_pages == 0`
(`src/bplustree.c:55-58`ff): it allocates page 0, writes the magic number
and an initial root, and pins/unpins it dirty like any other page — it
goes through the same buffer-pool/WAL path as tree nodes.
`update_root()` (`src/bplustree.c:1235-1245`) rewrites `root_id` on page 0
every time a split/merge changes which page is the root.

**Note:** the magic number is written but there's no code path that
currently *checks* it on open — see [known-issues.md](known-issues.md).

## Node pages

Every page other than page 0 holds one `Node` (`src/bplustree.h:12-29`),
written via `serialize_node()`:

```c
bool serialize_node(const Node *node, void *buffer) {
    memset(buffer, 0, PAGE_SIZE);
    memcpy(buffer, node, sizeof(Node));
    return true;
}
```
(`src/serialize.c:8-16`) — and read back with `deserialize_node()`, which
does the reverse `memcpy` into a freshly `malloc`'d `Node`.

**This means the on-disk format is a raw memory dump of the C struct**, not
an independently-defined byte layout. Concretely:

- `sizeof(Node)` is 80 bytes on the reference build (x86-64, gcc, default
  struct packing) — meaning **~98% of every 4096-byte page is zero-padding**.
  This is fine for correctness (pages are fixed-size by design so page ids
  map directly to file offsets) but means the file-on-disk is far larger
  than the data it holds. Not something to "optimize" without being asked
  — it's a direct consequence of the page-per-node design, not a bug.
- The format is **not portable across compilers/architectures/ABIs**:
  struct padding, `int` width, and endianness all leak directly into the
  file. A database file written by one build is only guaranteed readable
  by an identical build. Don't assume you can `scp` a `.db` file between a
  32-bit and 64-bit machine, or between different compilers, and have it
  read back correctly.
- There is **no format version field** on `Node` pages. If `MAX_KEYS`,
  field order, or field types in `Node` ever change, every existing
  database file silently becomes garbage on read (no error, just wrong
  data) — `deserialize_node` will happily `memcpy` misaligned bytes into
  the new struct shape.

## Growing the file

`pm_write_page()` (`src/page_manager.c:99-123`) will silently **grow the
file** if asked to write a `page_id >= num_pages`, by seeking to the last
byte of the target page and writing one byte to extend it, then updating
`num_pages`. This is deliberate and load-bearing: WAL recovery
(`wal_recover`, see [wal-recovery.md](wal-recovery.md)) calls
`pm_write_page` directly for pages that may not have been
`pm_allocate_page`'d yet in the *current* process's lifetime (they were
allocated in the crashed process). Don't "fix" this into an assertion —
recovery depends on it.

`pm_allocate_page()` (`src/page_manager.c:58-75`) is the normal path:
appends one zeroed page, returns its id. Returns `INVALID_PAGE_ID` on a
short write (disk full / I/O error) without incrementing `num_pages`.

## Reading/writing a page

`pm_read_page`/`pm_write_page` do a plain `lseek` + `read`/`write` of
exactly `PAGE_SIZE` bytes — no caching, no partial reads accepted (a short
read/write is treated as failure). All caching lives one layer up, in
`BufferPool`.
