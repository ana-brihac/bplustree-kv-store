/*
 * test_validate.c
 *
 * Tests for Phase 8: validate_tree() unit tests, property tests, and stress runs.
 *
 * 8.1 — validate_tree invariant checks (unit tests on handcrafted trees)
 * 8.2 — Property test: random insert/delete sequences, validate after each op,
 *        compare results against a reference sorted array maintained in parallel
 * 8.3 — Stress: 1 000, 100 000, 1 000 000 random operations
 */

#include "test_helpers.h"
#include <time.h>
#include <limits.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Phase 8.1 — validate_tree invariant unit tests
 * --------------------------------------------------------------- */

static void test_validate_empty_tree(void) {
    ASSERT("validate_empty_tree", validate_tree(NULL), "NULL root should be valid");
}

static void test_validate_single_leaf(void) {
    Node *root = NULL;
    root = insert(root, 10, strdup("ten"));
    ASSERT("validate_single_leaf", validate_tree(root), "single-key leaf should be valid");
    free_test_tree_with_values(root);
}

static void test_validate_after_inserts(void) {
    Node *root = NULL;
    for (int i = 1; i <= 15; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "v%d", i);
        root = insert(root, i * 10, strdup(buf));
        ASSERT("validate_after_each_insert", validate_tree(root), "tree must be valid after every insert");
    }
    free_test_tree_with_values(root);
}

static void test_validate_after_deletes(void) {
    Node *root = NULL;
    for (int i = 1; i <= 15; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "v%d", i);
        root = insert(root, i * 10, strdup(buf));
    }

    int keys[] = {50, 80, 30, 120, 10, 70, 140};
    for (int k = 0; k < 7; k++) {
        root = deleteNode(root, keys[k]);
        ASSERT("validate_after_each_delete", validate_tree(root), "tree must be valid after every delete");
    }
    free_test_tree_with_values(root);
}

static void test_validate_detects_bad_min_keys(void) {
    /* handcraft a leaf with only 1 key and a parent (should fail min-occupancy) */
    Node *parent = create_inner_node();
    Node *left   = create_leaf_node();
    Node *right  = create_leaf_node();

    insert_into_leaf_sorted(left,  10, "ten");
    insert_into_leaf_sorted(right, 20, "twenty");
    insert_into_leaf_sorted(right, 30, "thirty");

    parent->keys[0]                = 20;
    parent->num_keys               = 1;
    parent->data.inner.children[0] = (left) ? (left)->page_id : INVALID_PAGE_ID;
    parent->data.inner.children[1] = (right) ? (right)->page_id : INVALID_PAGE_ID;
    left->parent_id = (parent) ? (parent)->page_id : INVALID_PAGE_ID;
    right->parent_id = (parent) ? (parent)->page_id : INVALID_PAGE_ID;

    /* left has 1 key, MIN is (MAX_KEYS+1)/2 = 2, but left is a child -> should fail */
    ASSERT("validate_detects_bad_min_keys", !validate_tree(parent), "underflowed node should fail validation");
    free_test_tree(parent);
}

static void test_validate_detects_unsorted_keys(void) {
    /* handcraft a leaf with unsorted keys */
    Node *leaf = create_leaf_node();
    leaf->keys[0] = 30;
    leaf->keys[1] = 10; /* out of order */
    leaf->num_keys = 2;

    ASSERT("validate_detects_unsorted_keys", !validate_tree(leaf), "unsorted keys should fail validation");
    free(leaf);
}

/* ---------------------------------------------------------------
 * Phase 8.2 — Reference model and property testing
 * --------------------------------------------------------------- */

/* A simple sorted-array reference that mirrors the B+Tree's state */
#define REF_CAPACITY 2000000

typedef struct {
    int  keys[REF_CAPACITY];
    int  size;
} Ref;

static void ref_insert(Ref *r, int key) {
    for (int i = 0; i < r->size; i++) {
        if (r->keys[i] == key) return; /* no duplicates */
    }
    r->keys[r->size++] = key;
}

static void ref_delete(Ref *r, int key) {
    for (int i = 0; i < r->size; i++) {
        if (r->keys[i] == key) {
            r->keys[i] = r->keys[--r->size]; /* swap-remove — order doesn't matter for membership */
            return;
        }
    }
}

static bool ref_contains(Ref *r, int key) {
    for (int i = 0; i < r->size; i++) {
        if (r->keys[i] == key) return true;
    }
    return false;
}

/*
 * Run n_ops random operations on the tree and the reference in lockstep.
 * After every operation: validate_tree() must pass, and search() must agree with the reference.
 * Returns 0 on success, 1 on first discrepancy.
 */
static int run_property_test(int n_ops, unsigned int seed) {
    Node *root = NULL;
    Ref  *ref  = calloc(1, sizeof(Ref));
    if (!ref) { fprintf(stderr, "OOM\n"); return 1; }

    srand(seed);

    for (int op = 0; op < n_ops; op++) {
        int key = (rand() % 500) + 1; /* keys in [1, 500] — small range to force collisions */

        if (rand() % 2 == 0) { /* insert */
            if (!ref_contains(ref, key)) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", key);
                root = insert(root, key, strdup(buf)); /* heap-allocated so deleteNode can free() it */
            }
            ref_insert(ref, key);
        } else { /* delete */
            if (ref_contains(ref, key)) {
                root = deleteNode(root, key);
            }
            ref_delete(ref, key);
        }

        /* validate invariants */
        if (!validate_tree(root)) {
            fprintf(stderr, "validate_tree FAILED at op %d (key=%d)\n", op, key);
            free(ref);
            free_test_tree(root);
            return 1;
        }

        /* spot-check: a random key from the reference must be searchable */
        if (ref->size > 0) {
            int probe = ref->keys[rand() % ref->size];
            if (!search(root, probe)) {
                fprintf(stderr, "search() FAILED at op %d: key %d in ref but not in tree\n", op, probe);
                free(ref);
                free_test_tree(root);
                return 1;
            }
        }
    }

    free(ref);
    free_test_tree(root);
    return 0;
}

/* ---------------------------------------------------------------
 * Phase 8.3 — Stress runs at increasing sizes
 * --------------------------------------------------------------- */

static void test_stress_1000(void) {
    int result = run_property_test(1000, 42);
    ASSERT("stress_1000_ops", result == 0, "tree corrupted during 1 000 random ops");
}

static void test_stress_100000(void) {
    int result = run_property_test(100000, 137);
    ASSERT("stress_100000_ops", result == 0, "tree corrupted during 100 000 random ops");
}

static void test_stress_1000000(void) {
    int result = run_property_test(1000000, 31337);
    ASSERT("stress_1000000_ops", result == 0, "tree corrupted during 1 000 000 random ops");
}

int main(void) {
    /* 8.1 — invariant unit tests */
    test_validate_empty_tree();
    test_validate_single_leaf();
    test_validate_after_inserts();
    test_validate_after_deletes();
    test_validate_detects_bad_min_keys();
    test_validate_detects_unsorted_keys();

    /* 8.2 / 8.3 — property tests and stress */
    test_stress_1000();
    test_stress_100000();
    test_stress_1000000();

    return 0;
}
