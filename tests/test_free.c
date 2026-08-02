/*
 * test_free.c
 *
 * Tests for: freeNode
 *
 * Correctness here is primarily verified by AddressSanitizer (ASAN):
 * if freeNode double-frees, leaks, or accesses freed memory, ASAN will
 * catch it and the test binary will exit with a non-zero code.
 *
 * Cases:
 *   free_null_no_crash       - freeNode(NULL) does not crash
 *   free_leaf_no_crash       - freeNode on a leaf with values does not crash
 *   free_inner_no_crash      - freeNode on an inner node + children does not crash
 *   free_single_key_leaf     - leaf with one key is freed cleanly
 */

#include "test_helpers.h"

static void test_free_null(void) {
    freeNode(NULL); /* should be a no-op */
    PASS("free_null_no_crash");
}

static void test_free_leaf(void) {
    Node *leaf = create_leaf_node();
    leaf->num_keys = 2;
    leaf->keys[0] = 1; leaf->data.leaf.values[0] = (int64_t)(NULL); /* no heap value */
    leaf->keys[1] = 2; leaf->data.leaf.values[1] = (int64_t)(NULL);

    freeNode(leaf); /* ASAN will complain if something goes wrong */
    PASS("free_leaf_no_crash");
}

static void test_free_single_key_leaf(void) {
    Node *leaf = create_leaf_node();
    leaf->num_keys = 1;
    leaf->keys[0] = 42;
    leaf->data.leaf.values[0] = (int64_t)(NULL);

    freeNode(leaf);
    PASS("free_single_key_leaf");
}

static void test_free_inner(void) {
    Node *root  = create_inner_node();
    Node *left  = create_leaf_node();
    Node *right = create_leaf_node();

    left->num_keys  = 1; left->keys[0]  = 5;  left->data.leaf.values[0] = (int64_t)(NULL);
    right->num_keys = 1; right->keys[0] = 15; right->data.leaf.values[0] = (int64_t)(NULL);

    root->keys[0] = 15;
    root->num_keys = 1;
    root->data.inner.children[0] = (left) ? (left)->page_id : INVALID_PAGE_ID;
    root->data.inner.children[1] = (right) ? (right)->page_id : INVALID_PAGE_ID;

    freeNode(root); /* recursively frees left, right, then root */
    PASS("free_inner_no_crash");
}

int main(void) {
    test_free_null();
    test_free_leaf();
    test_free_single_key_leaf();
    test_free_inner();
    return 0;
}
