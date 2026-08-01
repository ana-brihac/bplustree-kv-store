# bplustree-kv-store

A persistent, embedded key-value store written in C, implementing an on-disk B+Tree with page-based storage and an LRU buffer pool — inspired by the design of embedded engines like LMDB and SQLite's storage layer.

## Status: 🚧 In progress (Moving to Disk & Buffer Pool)
The core in-memory B+Tree structure is fully complete and heavily tested! 🎉 Next up is the page manager and LRU buffer pool to persist this beauty to disk.

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
* `createTree()` — Initializes an empty B+Tree structure.
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

To run the tests, simply execute:
```bash
make test
```

### What does `make test` do?
1. It compiles all the individual test files located in the `tests/` directory into the `tests/bin/` folder.
2. It executes them one by one.
3. **Property & Stress Testing**: Tests like `test_validate` run massive randomized simulations (up to 1,000,000 randomized inserts and deletes), calling `validate_tree` after *every single operation* to ensure invariants are never broken.
4. **Memory Leak Checks**: Everything is compiled with `-fsanitize=address`. If there is a single memory leak, dangling pointer, or out-of-bounds array access, the tests will immediately crash and report it.
5. **Output Verification**: The outputs of the tests are automatically diffed against the golden reference files in `tests/expected/`.
