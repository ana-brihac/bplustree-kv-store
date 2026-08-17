#include "test_helpers.h"
#include "../src/buffer_pool.h"
#include "../src/page_manager.h"
#include "../src/serialize.h"
#include <stdint.h>
#include <stdlib.h>

/* T-5: verify merge_with_sibling refuses to merge when the combined
 * key count would exceed MAX_KEYS (merged > MAX_KEYS check, line 882).
 *
 * We build two leaf nodes each holding MAX_KEYS keys and a minimal
 * parent, then call merge_with_sibling directly.  The function must
 * return NULL (merge refused) and leave both nodes untouched.
 */
static void test_merge_overflow_guard(void) {
    remove("test_merge_g.db");
    remove("test_merge_g.wal");

    Tree *tree = tree_open("test_merge_g.db", "test_merge_g.wal");
    if (!tree) { FAIL("merge_overflow_guard_open", "tree_open returned NULL"); return; }

    BufferPool *bp = tree->bp;

    // --- build left leaf: keys 0..MAX_KEYS-1 ---
    page_id_t left_id = create_leaf_node(bp);
    void *left_raw = bp_fetch_page(bp, left_id);
    Node *left = deserialize_node(left_raw);
    for (int i = 0; i < MAX_KEYS; i++) {
        left->keys[i] = i;
        left->data.leaf.values[i] = (int64_t)i;
    }
    left->num_keys = MAX_KEYS;

    // --- build right leaf: keys MAX_KEYS..2*MAX_KEYS-1 ---
    page_id_t right_id = create_leaf_node(bp);
    void *right_raw = bp_fetch_page(bp, right_id);
    Node *right = deserialize_node(right_raw);
    for (int i = 0; i < MAX_KEYS; i++) {
        right->keys[i] = MAX_KEYS + i;
        right->data.leaf.values[i] = (int64_t)(MAX_KEYS + i);
    }
    right->num_keys = MAX_KEYS;

    // --- build a minimal inner parent with one key ---
    page_id_t par_id = create_inner_node(bp);
    void *par_raw = bp_fetch_page(bp, par_id);
    Node *par = deserialize_node(par_raw);
    par->keys[0] = MAX_KEYS;
    par->data.inner.children[0] = left_id;
    par->data.inner.children[1] = right_id;
    par->num_keys = 1;
    left->parent_id = par_id;
    right->parent_id = par_id;

    // --- attempt the merge (must be refused because MAX_KEYS + MAX_KEYS > MAX_KEYS) ---
    int left_keys_before  = left->num_keys;
    int right_keys_before = right->num_keys;
    int par_keys_before   = par->num_keys;

    Node *result = merge_with_sibling(bp, left, right, par, 0);

    ASSERT("merge_overflow_returns_null", result == NULL,
           "merge_with_sibling must return NULL when merged > MAX_KEYS");
    ASSERT_INT_EQ("merge_overflow_left_unchanged",  left_keys_before,  left->num_keys);
    ASSERT_INT_EQ("merge_overflow_right_unchanged", right_keys_before, right->num_keys);
    ASSERT_INT_EQ("merge_overflow_parent_unchanged", par_keys_before,  par->num_keys);

    // --- clean up ---
    bp_unpin(bp, left_id,  false); free(left);
    bp_unpin(bp, right_id, false); free(right);
    bp_unpin(bp, par_id,   false); free(par);

    tree_close(tree);
    remove("test_merge_g.db");
    remove("test_merge_g.wal");
}

/* Sanity-check the normal case: merge IS allowed when combined count <= MAX_KEYS.
 * With MAX_KEYS=4, two leaves each with 2 keys (2+2=4) should succeed.
 */
static void test_merge_normal_succeeds(void) {
    remove("test_merge_n.db");
    remove("test_merge_n.wal");

    Tree *tree = tree_open("test_merge_n.db", "test_merge_n.wal");
    if (!tree) { FAIL("merge_normal_open", "tree_open returned NULL"); return; }

    BufferPool *bp = tree->bp;
    int half = MAX_KEYS / 2; // 2 for MAX_KEYS=4

    page_id_t left_id = create_leaf_node(bp);
    void *left_raw = bp_fetch_page(bp, left_id);
    Node *left = deserialize_node(left_raw);
    for (int i = 0; i < half; i++) {
        left->keys[i] = i;
        left->data.leaf.values[i] = (int64_t)i;
    }
    left->num_keys = half;

    page_id_t right_id = create_leaf_node(bp);
    void *right_raw = bp_fetch_page(bp, right_id);
    Node *right = deserialize_node(right_raw);
    for (int i = 0; i < half; i++) {
        right->keys[i] = half + i;
        right->data.leaf.values[i] = (int64_t)(half + i);
    }
    right->num_keys = half;

    page_id_t par_id = create_inner_node(bp);
    void *par_raw = bp_fetch_page(bp, par_id);
    Node *par = deserialize_node(par_raw);
    par->keys[0] = half;
    par->data.inner.children[0] = left_id;
    par->data.inner.children[1] = right_id;
    par->num_keys = 1;
    left->parent_id = par_id;
    right->parent_id = par_id;

    Node *result = merge_with_sibling(bp, left, right, par, 0);

    ASSERT("merge_normal_returns_nonnull", result != NULL,
           "merge_with_sibling must succeed when merged <= MAX_KEYS");
    ASSERT_INT_EQ("merge_normal_left_has_all_keys", half * 2, left->num_keys);

    bp_unpin(bp, left_id,  true); free(left);
    bp_unpin(bp, right_id, false); free(right);
    bp_unpin(bp, par_id,   true); free(par);

    tree_close(tree);
    remove("test_merge_n.db");
    remove("test_merge_n.wal");
}

int main(void) {
    test_merge_overflow_guard();
    test_merge_normal_succeeds();
    return 0;
}
