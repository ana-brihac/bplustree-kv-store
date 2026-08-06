/*
 * test_search_leaf.c
 *
 * Tests for: search — against a single leaf node
 *
 * Cases:
 *   search_null_root             - search on NULL returns NULL
 *   search_leaf_found_first      - find the first key in the leaf
 *   search_leaf_found_middle     - find a key in the middle
 *   search_leaf_found_last       - find the last key in the leaf
 *   search_leaf_value_correct    - returned pointer equals the stored value
 *   search_leaf_missing          - key not in leaf returns NULL
 *   search_leaf_below_range      - key smaller than all keys returns NULL
 *   search_leaf_above_range      - key larger than all keys returns NULL
 */

#include "test_helpers.h"

static void test_search_null(void) {
    ASSERT_NULL("search_null_root", search(NULL, 42));
}

static void test_search_on_leaf(void) {
    Node *leaf = create_leaf_node();
    leaf->num_keys = 3;
    leaf->keys[0] = 10; leaf->data.leaf.values[0] = (int64_t)("ten");
    leaf->keys[1] = 20; leaf->data.leaf.values[1] = (int64_t)("twenty");
    leaf->keys[2] = 30; leaf->data.leaf.values[2] = (int64_t)("thirty");

    ASSERT_STR_EQ("search_leaf_found_first",  "ten",    search(leaf, 10));
    ASSERT_STR_EQ("search_leaf_found_middle", "twenty", search(leaf, 20));
    ASSERT_STR_EQ("search_leaf_found_last",   "thirty", search(leaf, 30));

    /* The pointer returned must be the exact same pointer we stored */
    void *stored = leaf->data.leaf.values[1];
    ASSERT("search_leaf_value_correct",
           search(leaf, 20) == stored,
           "returned pointer should equal the stored pointer");

    ASSERT_NULL("search_leaf_missing",       search(leaf,  0));
    ASSERT_NULL("search_leaf_below_range",   search(leaf,  5));
    ASSERT_NULL("search_leaf_above_range",   search(leaf, 99));

    free_test_tree(leaf);
}

int main(void) {
    test_search_null();
    test_search_on_leaf();
    return 0;
}
