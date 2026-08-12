# bplustree-kv-store

A persistent, embedded key-value store written in C, implementing an on-disk B+Tree with page-based storage, an LRU buffer pool, and a Write-Ahead Log for crash recovery — inspired by the design of embedded engines like LMDB and SQLite's storage layer.

## Status: 🚧 In progress (Write-Ahead Log)
The core B+Tree, on-disk page manager, and LRU buffer pool are all fully implemented and heavily tested! 🎉 The Write-Ahead Log (WAL) for crash recovery is the current focus.

## ✨ Features (So far)

### 🌳 The In-Memory B+Tree
A fully compliant, classic B+Tree built from scratch in C. 
* **Classic node design**: Inner nodes for routing, leaf nodes for data. All leaves are linked together at the same depth.
* **O(log n) operations**: Fast inserts, exact-match searches, and deletions.
* **Range queries**: Jump to the start key and ride the leaf sibling pointers to quickly collect ranges of data.
* **Robust rebalancing**: Handles proactive splits on the way down, node merging, and sibling key-borrowing on deletion to maintain tree invariants perfectly.

### 💾 Page Manager
All tree data is persisted to a binary file on disk. The page manager owns the file and provides a clean read/write interface over fixed-size pages (`PAGE_SIZE = 4096` bytes).
* **Page allocation**: `pm_allocate_page(pm)` — Appends a fresh zeroed page to the file and returns its `page_id`.
* **Raw page I/O**: `pm_read_page(pm, page_id, buffer)` / `pm_write_page(pm, page_id, buffer)` — Reads or writes exactly one `PAGE_SIZE` chunk at the correct file offset.
* **Open / close**: `pm_open(filename)` — Opens or creates the database file and recovers `num_pages` from the file size. `pm_close(pm)` — Flushes and frees everything.

### ⚡ LRU Buffer Pool
A `BUFFER_POOL_SIZE = 64` frame cache that sits between the B+Tree and the page manager, so hot pages stay in memory instead of being read from disk every time.
* **LRU eviction**: When all frames are occupied, the frame with the oldest `last_time_used` timestamp gets flushed and recycled.
* **Dirty tracking**: Frames are only written back to disk when they are actually modified (`mark_dirty = true` on unpin), avoiding unnecessary I/O.
* **O(1) lookup**: An open-addressed hash table (`HASH_TABLE_SIZE = 127`) maps `page_id → frame_index` in constant time.
* **Pin / unpin contract**: Callers pin a page before using it and unpin it when done. The eviction policy only considers unpinned frames, so a page can never be evicted while it is actively in use.

### 📝 Write-Ahead Log (WAL)
A redo-only append-only log that records every page write before it hits the buffer pool. Designed so that after a crash, the database can be recovered by replaying the log in sequence-number order.
* **`wal_open(filename)`** — Opens or creates the WAL file. If the file already exists, `next_seq_num` is recovered from the file size so sequence numbers never collide with existing records.
* **`wal_append(wal, page_id, page_data)`** — Appends a new `WALRecord` (containing `page_id`, `seq_num`, and a full `PAGE_SIZE` page snapshot) to the log. The sequence number is assigned atomically and the handle counter is incremented.
* **`wal_fsync(wal)`** — Forces the OS to flush all buffered log data to the physical disk. This is the call that actually provides the durability guarantee.
* **`wal_close(wal)`** — Implicitly calls `wal_fsync` before closing the file descriptor and freeing the handle.

## 🛠️ API & Core Functions

Here is a quick breakdown of the core functions (found in `src/`):

### B+Tree (`src/bplustree.c`)
* `createTree()` — Initializes an empty B+Tree structure.
* `insert(Node *root, int key, void *value)` — Inserts a new key-value pair into the tree. If a node gets too full (hits `MAX_KEYS`), it automatically triggers a split and pushes the median key up to the parent.
* `search(Node *root, int key)` — Traverses down the inner nodes to find the exact leaf containing the key and returns its value. Returns `NULL` if not found.
* `deleteNode(Node *root, int key)` — Removes a key from the tree. If a node drops below the minimum required keys (half full), it automatically borrows a key from a sibling or merges with one to maintain a perfectly balanced tree.
* `range_search(Node *node, int start, int end, ...)` — Finds the start key, then walks rightward along the leaf sibling pointers (`node->data.leaf.next`), collecting all keys and values until it passes `end`.

### Node Management & Splits
* `create_leaf_node()` / `create_inner_node()` — Memory allocation helpers for the two types of nodes.
* `split_leaf(BufferPool *bp, Node *leaf, void **new_raw)` / `split_inner_node(Node *node)` — Splits an overfull node into two halves.
* `insert_into_parent(...)` — Recursively handles pushing a promoted key up into the parent node after a split.

### Deletion Rebalancing
* `try_borrow_from_left_sibling(...)` / `try_borrow_from_right_sibling(...)` — Attempts to steal a key from an adjacent sibling if it has more than the minimum required keys.
* `merge_with_sibling(...)` — If siblings are too sparse to borrow from, this merges two nodes into one and removes the separating key from the parent.

### Validation
* `validate_tree(Node *root)` — A strict invariant checker. It recursively verifies that all leaves are at the exact same depth, nodes respect minimum/maximum key limits, and keys are strictly sorted.

## 🧪 Testing

This project includes a rigorous, AddressSanitizer-backed test suite that stresses every edge case of the tree, the storage layer, the buffer pool, and the WAL.

To run all tests, simply execute:
```bash
make test
```

### What does `make test` do?
1. It compiles all the individual test files located in the `tests/` directory into the `tests/bin/` folder.
2. It executes them one by one.
3. **Property & Stress Testing**: Tests like `test_validate` run massive randomized simulations (up to 1,000,000 randomized inserts and deletes), calling `validate_tree` after *every single operation* to ensure invariants are never broken.
4. **Memory Leak Checks**: Everything is compiled with `-fsanitize=address`. If there is a single memory leak, dangling pointer, or out-of-bounds array access, the tests will immediately crash and report it.
5. **Output Verification**: The outputs of the tests are automatically diffed against the golden reference files in `tests/expected/`.

### Test suites
| Suite | What it covers |
|---|---|
| `test_create` | Tree initialization |
| `test_insert_basic` | Basic insert and search |
| `test_insert_no_split` | Inserts that don't trigger a split |
| `test_insert_into_leaf_sorted` | Key ordering within a leaf |
| `test_insert_with_split` | Leaf and inner node splits |
| `test_split_leaf` | Split correctness, key distribution, sibling pointers |
| `test_search_leaf` / `test_search_tree` | Exact-match search at leaf and tree level |
| `test_delete` | Deletion with borrowing and merging |
| `test_range_search` | Range query correctness |
| `test_validate` | Full tree invariant validation + stress tests |
| `test_page_manager` | Disk page read/write roundtrip |
| `test_buffer_pool` | LRU eviction, dirty flushing, pin/unpin |
| `test_wal` | WAL open/append/fsync/close, sequence number continuity, error handling |
