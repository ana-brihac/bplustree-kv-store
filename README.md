# bplustree-kv-store

A persistent, embedded key-value store written in C, implementing an on-disk B+Tree with page-based storage and an LRU buffer pool — inspired by the design of embedded engines like LMDB and SQLite's storage layer.

## Status: ✅ Complete (Fully Persistent with WAL Recovery)
The core in-memory B+Tree structure is fully complete and heavily tested! 🎉
We have successfully implemented the Page Manager, LRU Buffer Pool, Write-Ahead Logging (WAL) for durability, and crash recovery.

## ✨ Features (So far)

### 🌳 The In-Memory B+Tree
A fully compliant, classic B+Tree built from scratch in C. 
* **Classic node design**: Inner nodes for routing, leaf nodes for data. All leaves are linked together at the same depth.
* **O(log n) operations**: Fast inserts, exact-match searches, and deletions.
* **Range queries**: Jump to the start key and ride the leaf sibling pointers to quickly collect ranges of data.
* **Robust rebalancing**: Handles proactive splits on the way down, node merging, and sibling key-borrowing on deletion to maintain tree invariants perfectly.

## 🛠️ API & Core Functions

Here is a quick breakdown of the core functions powering the B+Tree (found in `src/bplustree.c`):

### Core Operations
* `tree_open(const char *filename, const char *wal_filename)` / `tree_close(Tree *tree)` — Opens or creates the persistent B+Tree and WAL, and safely shuts it down.
* `insert(Node *root, int key, void *value)` — Inserts a new key-value pair into the tree. If a node gets too full (hits `MAX_KEYS`), it automatically triggers a split and pushes the median key up to the parent.
* `search(Node *root, int key)` — Traverses down the inner nodes to find the exact leaf containing the key and returns its value. Returns `NULL` if not found.
* `deleteNode(Node *root, int key)` — Removes a key from the tree. If a node drops below the minimum required keys (half full), it automatically borrows a key from a sibling or merges with one to maintain a perfectly balanced tree.
* `range_search(Node *node, int start, int end, ...)` — Finds the start key, then walks rightward along the leaf sibling pointers (`node->data.leaf.next`), collecting all keys and values until it passes `end`.

### Node Management & Splits
* `create_leaf_node()` / `create_inner_node()` — Memory allocation helpers for the two types of nodes.
* `split_leaf(Node *leaf)` / `split_inner_node(Node *node)` — Splits an overfull node into two halves.
* `insert_into_parent(...)` — Recursively handles pushing a promoted key up into the parent node after a split.

### Deletion Rebalancing
* `try_borrow_from_left_sibling(...)` / `try_borrow_from_right_sibling(...)` — Attempts to steal a key from an adjacent sibling if it has more than the minimum required keys.
* `merge_with_sibling(...)` — If siblings are too sparse to borrow from, this merges two nodes into one and removes the separating key from the parent.

### Validation
* `validate_tree(Node *root)` — A strict invariant checker. It recursively verifies that all leaves are at the exact same depth, nodes respect minimum/maximum key limits, and keys are strictly sorted.

## 🧪 Testing the B+Tree

This project includes a rigorous, AddressSanitizer-backed test suite that stresses every edge case of the tree.

To run the unit + integration tests:
```bash
make test
```

To run the kill-9 chaos tests (requires Python 3):
```bash
make chaos   # 50 random SIGKILL cycles — verifies crash recovery
make stress  # 100 random SIGKILL cycles — larger workload
```

### What does `make test` do?
1. Compiles all test files in `tests/` into `tests/bin/` with `-fsanitize=address`.
2. Executes them one by one and diffs output against golden files in `tests/expected/`.
3. **Property & Stress Testing**: `test_validate` runs massive randomized simulations (up to 1,000,000 inserts and deletes), calling `validate_tree` after *every single operation*.
4. **WAL & Recovery**: `test_recovery` verifies that data inserted before a simulated crash is fully restored on reopen. `test_checkpoint` additionally verifies data survives a crash immediately after a checkpoint (WAL already cleared, DB file must be complete). `test_reopen` verifies `root_id` persistence across a clean close/reopen cycle.
5. **Merge Guard**: `test_merge_guard` verifies that `merge_with_sibling` refuses to merge when the combined key count would exceed `MAX_KEYS`, and that a valid merge still succeeds.

### What do `make chaos` / `make stress` do?
Launch a workload binary, let it run for a random 10–200 ms, then `kill -9` it. On each cycle, a verifier re-opens the database and checks that all previously committed data is intact. These tests exercise the exact torn-write scenarios that checksummed WAL records are designed to handle — something the in-process test suite cannot replicate.

## Stress Testing

✅ Survived 100 random crash-recovery cycles with zero data corruption or loss.
