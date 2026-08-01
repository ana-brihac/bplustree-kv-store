#ifndef BPLUSTREE_H
#define BPLUSTREE_H

#include <stdbool.h>

#define MAX_KEYS 4

typedef struct Node {
    struct Node *parent;
	int keys[MAX_KEYS];
	int num_keys;
	bool is_leaf;

	union {
        struct {
            void *values[MAX_KEYS];   
            struct Node *next;    
        } leaf;
        
        struct {
            struct Node *children[MAX_KEYS + 1];
        } inner;
    } data;
} Node;

typedef struct Tree {
	Node *root;
} Tree;

Tree *createTree();
Node *insert(Node *root, int key, void *value);
void *search(Node *root, int key);
Node *deleteNode(Node *root, int key);
Node *create_leaf_node();
Node *create_inner_node();
void freeNode(Node *node);
int find_key_in_node(Node *node, int key);
Node *find_child_index(Node *inner_node, int key);
Node *remove_from_leaf(Node *leaf, int key);
void insert_into_leaf_sorted(Node *leaf, int key, void *value);
Node *split_leaf(Node *leaf);
Node *split_inner_node(Node *node);
Node *insert_into_parent(Node *parent, Node *left_child, int key, Node *right_child);
bool try_borrow_from_left_sibling(Node *node, Node *parent, int index);
bool try_borrow_from_right_sibling(Node *node, Node *parent, int index);
Node *merge_with_sibling(Node *node, Node *sibling, Node *parent, int index);
Node *remove_from_parent(Node *parent, int index);
bool validate_tree(Node *root);
bool key_check_less(Node *a, int n);
bool key_check_greater_eq(Node *a, int n);

#endif

/*Phase 8 — Validation & stress testing

8.1 validate_tree(root) — checks all B+Tree invariants: all leaves same depth, every inner node has k+1 children for k keys, all nodes (except root) at least half full, keys properly sorted
8.2 Property test: generate random sequences of insert/delete operations, call validate_tree() after every operation, compare results against a reference (std::map-equivalent or just a sorted array you maintain in parallel)
8.3 Run with 1000, then 100,000, then 1,000,000 random operations. Any invariant violation = bug to hunt down.

✅ Checkpoint: your tree survives a million random operations without corrupting.*/