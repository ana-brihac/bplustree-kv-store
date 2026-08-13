/*
 * test_create.c
 *
 * Tests for: createTree, create_leaf_node, create_inner_node
 *
 * Cases:
 *   create_tree_returns_non_null      - createTree() never returns NULL
 *   create_tree_root_is_null          - the root field starts as NULL
 *   create_leaf_is_leaf               - is_leaf flag is true
 *   create_leaf_num_keys_zero         - num_keys starts at 0
 *   create_leaf_values_are_null       - all value slots are NULL
 *   create_leaf_next_is_null          - next pointer is NULL
 *   create_inner_not_leaf             - is_leaf flag is false
 *   create_inner_num_keys_zero        - num_keys starts at 0
 *   create_inner_children_are_null    - all children slots are NULL
 */

#include "test_helpers.h"
#include "../src/buffer_pool.h"
#include "../src/page_manager.h"
#include "../src/serialize.h"

static void test_createTree(void) {
    Tree *t = createTree();
    ASSERT_NOT_NULL("create_tree_returns_non_null", t);
    ASSERT("create_tree_root_is_null", t->root_id == INVALID_PAGE_ID, "root should be INVALID_PAGE_ID");
    free(t);
}

static void test_create_leaf_node(void) {
    PageManager *pm = pm_open("test_create_leaf.db");
    BufferPool *bp = bp_create(pm, "test_wal.log");
    page_id_t leaf_id = create_leaf_node(bp);
    ASSERT_NOT_NULL("create_leaf_returns_valid_id", leaf_id != INVALID_PAGE_ID ? (void*)1 : NULL);
    
    void *raw = bp_fetch_page(bp, leaf_id);
    Node *leaf = deserialize_node(raw);
    
    ASSERT("create_leaf_is_leaf",      leaf->is_leaf  == true,  "is_leaf should be true");
    ASSERT_INT_EQ("create_leaf_num_keys_zero", 0, leaf->num_keys);
    ASSERT("create_leaf_next_is_null", leaf->data.leaf.next_id == INVALID_PAGE_ID, "next_id should be INVALID_PAGE_ID");

    int all_null = 1;
    for (int i = 0; i < MAX_KEYS; i++) {
        if (leaf->data.leaf.values[i] != 0) { all_null = 0; break; }
    }
    ASSERT("create_leaf_values_are_null", all_null, "all value slots should be NULL");

    bp_unpin(bp, leaf_id, false);
    free(leaf);
    bp_destroy(bp);
    pm_close(pm);
    remove("test_create_leaf.db");
}

static void test_create_inner_node(void) {
    PageManager *pm = pm_open("test_create_inner.db");
    BufferPool *bp = bp_create(pm, "test_wal.log");
    page_id_t inner_id = create_inner_node(bp);
    ASSERT_NOT_NULL("create_inner_returns_valid_id", inner_id != INVALID_PAGE_ID ? (void*)1 : NULL);

    void *raw = bp_fetch_page(bp, inner_id);
    Node *inner = deserialize_node(raw);

    ASSERT("create_inner_not_leaf",    inner->is_leaf == false, "is_leaf should be false");
    ASSERT_INT_EQ("create_inner_num_keys_zero", 0, inner->num_keys);

    int all_null = 1;
    for (int i = 0; i <= MAX_KEYS; i++) {
        if (inner->data.inner.children[i] != INVALID_PAGE_ID) { all_null = 0; break; }
    }
    ASSERT("create_inner_children_are_null", all_null, "all children slots should be NULL");

    bp_unpin(bp, inner_id, false);
    free(inner);
    bp_destroy(bp);
    pm_close(pm);
    remove("test_create_inner.db");
}

int main(void) {
    test_createTree();
    test_create_leaf_node();
    test_create_inner_node();
    return 0;
}
