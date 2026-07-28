/*
 * test_insert_no_split.c
 *
 * Tests for: insert — filling a leaf to capacity without splitting
 *
 * Cases:
 *   fill_leaf_num_keys_3      - three items fit in leaf
 *   fill_leaf_still_leaf      - node remains a leaf after 3 inserts
 *   fill_leaf_keys_sorted     - all inserted keys are in sorted order
 *   fill_leaf_max_num_keys    - MAX_KEYS-1 items inserted without split
 *   fill_leaf_max_still_leaf  - node is still a leaf at MAX_KEYS-1 items
 *   fill_full_leaf_num_keys   - exactly MAX_KEYS items fit when leaf is fresh
 *   fill_full_search_mid      - search returns correct value after full fill
 */

#include "test_helpers.h"

static void test_fill_three(void) {
    Node *root = NULL;
    root = insert(root, 10, "v1");
    root = insert(root, 20, "v2");
    root = insert(root, 30, "v3");

    ASSERT_INT_EQ("fill_leaf_num_keys_3", 3, root->num_keys);
    ASSERT("fill_leaf_still_leaf", root->is_leaf == true, "should still be a leaf");

    /* keys sorted */
    ASSERT_INT_EQ("fill_leaf_key0", 10, root->keys[0]);
    ASSERT_INT_EQ("fill_leaf_key1", 20, root->keys[1]);
    ASSERT_INT_EQ("fill_leaf_key2", 30, root->keys[2]);

    free_test_tree(root);
}

static void test_fill_max_minus_one(void) {
    /* MAX_KEYS is 4, so fill 3 keys (MAX_KEYS - 1) */
    Node *root = NULL;
    root = insert(root, 100, "a");
    root = insert(root, 50,  "b");
    root = insert(root,  75, "c");

    ASSERT_INT_EQ("fill_leaf_max_num_keys",   3, root->num_keys);
    ASSERT("fill_leaf_max_still_leaf", root->is_leaf == true, "should still be a leaf");

    free_test_tree(root);
}

static void test_fill_and_search(void) {
    /* Insert MAX_KEYS items directly via insert_into_leaf_sorted to bypass
       the capacity check in insert(), then verify search works */
    Node *leaf = create_leaf_node();
    insert_into_leaf_sorted(leaf, 5,  "five");
    insert_into_leaf_sorted(leaf, 15, "fifteen");
    insert_into_leaf_sorted(leaf, 25, "twenty-five");
    insert_into_leaf_sorted(leaf, 35, "thirty-five");   /* fills to MAX_KEYS=4 */

    ASSERT_INT_EQ("fill_full_leaf_num_keys", MAX_KEYS, leaf->num_keys);
    ASSERT_STR_EQ("fill_full_search_mid", "fifteen", search(leaf, 15));
    ASSERT_STR_EQ("fill_full_search_last", "thirty-five", search(leaf, 35));
    ASSERT_NULL("fill_full_search_missing", search(leaf, 99));

    free_test_tree(leaf);
}

int main(void) {
    test_fill_three();
    test_fill_max_minus_one();
    test_fill_and_search();
    return 0;
}
