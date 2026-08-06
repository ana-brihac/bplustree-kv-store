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

static void test_createTree(void) {
    Tree *t = createTree();
    ASSERT_NOT_NULL("create_tree_returns_non_null", t);
    ASSERT_NULL("create_tree_root_is_null", t->root);
    free(t);
}

static void test_create_leaf_node(void) {
    Node *leaf = create_leaf_node();
    ASSERT_NOT_NULL("create_leaf_returns_non_null", leaf);
    ASSERT("create_leaf_is_leaf",      leaf->is_leaf  == true,  "is_leaf should be true");
    ASSERT_INT_EQ("create_leaf_num_keys_zero", 0, leaf->num_keys);
    ASSERT_NULL("create_leaf_next_is_null", fetch_node(leaf->data.leaf.next_id));

    int all_null = 1;
    for (int i = 0; i < MAX_KEYS; i++) {
        if (leaf->data.leaf.values[i] != 0) { all_null = 0; break; }
    }
    ASSERT("create_leaf_values_are_null", all_null, "all value slots should be NULL");

    free(leaf);
}

static void test_create_inner_node(void) {
    Node *inner = create_inner_node();
    ASSERT_NOT_NULL("create_inner_returns_non_null", inner);
    ASSERT("create_inner_not_leaf",    inner->is_leaf == false, "is_leaf should be false");
    ASSERT_INT_EQ("create_inner_num_keys_zero", 0, inner->num_keys);

    int all_null = 1;
    for (int i = 0; i <= MAX_KEYS; i++) {
        if (fetch_node(inner->data.inner.children[i]) != 0) { all_null = 0; break; }
    }
    ASSERT("create_inner_children_are_null", all_null, "all children slots should be NULL");

    free(inner);
}

int main(void) {
    test_createTree();
    test_create_leaf_node();
    test_create_inner_node();
    return 0;
}
