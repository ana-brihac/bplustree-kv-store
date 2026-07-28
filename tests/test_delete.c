/*
 * test_delete.c
 *
 * Tests for: deleteNode
 *
 * Note: deleteNode calls free() on the removed value, so all values
 * here are heap-allocated with strdup().
 *
 * Cases:
 *   delete_existing_decrements_num_keys  - num_keys goes from 3 to 2
 *   delete_existing_key_gone             - searching the deleted key returns NULL
 *   delete_existing_others_remain        - the other keys are still searchable
 *   delete_shifts_keys_left              - the key array shifts correctly after delete
 *   delete_missing_key_unchanged         - deleting a key not in tree changes nothing
 *   delete_from_null                     - deleteNode on NULL root returns NULL
 */

#include "test_helpers.h"

/* Build a 3-key leaf with heap-allocated values */
static Node *build_leaf_heap(void) {
    Node *leaf = create_leaf_node();
    insert_into_leaf_sorted(leaf, 10, strdup("ten"));
    insert_into_leaf_sorted(leaf, 20, strdup("twenty"));
    insert_into_leaf_sorted(leaf, 30, strdup("thirty"));
    return leaf;
}

static void test_delete_existing(void) {
    Node *root = build_leaf_heap();

    root = deleteNode(root, 20);

    ASSERT_INT_EQ("delete_existing_decrements_num_keys", 2, root->num_keys);
    ASSERT_NULL("delete_existing_key_gone", search(root, 20));
    ASSERT_STR_EQ("delete_existing_others_remain_10", "ten",    search(root, 10));
    ASSERT_STR_EQ("delete_existing_others_remain_30", "thirty", search(root, 30));

    /* After deleting key 20 (index 1), keys should be [10, 30] */
    ASSERT_INT_EQ("delete_shifts_keys_key0", 10, root->keys[0]);
    ASSERT_INT_EQ("delete_shifts_keys_key1", 30, root->keys[1]);

    free_test_tree_with_values(root);
}

static void test_delete_first_key(void) {
    Node *root = build_leaf_heap();

    root = deleteNode(root, 10);

    ASSERT_INT_EQ("delete_first_decrements_num_keys", 2, root->num_keys);
    ASSERT_NULL("delete_first_key_gone", search(root, 10));

    /* Remaining keys should shift: [20, 30] */
    ASSERT_INT_EQ("delete_first_shifts_key0", 20, root->keys[0]);
    ASSERT_INT_EQ("delete_first_shifts_key1", 30, root->keys[1]);

    free_test_tree_with_values(root);
}

static void test_delete_missing(void) {
    Node *root = build_leaf_heap();

    root = deleteNode(root, 999); /* key doesn't exist */

    ASSERT_INT_EQ("delete_missing_key_unchanged", 3, root->num_keys);

    free_test_tree_with_values(root);
}

static void test_delete_null(void) {
    Node *result = deleteNode(NULL, 10);
    ASSERT_NULL("delete_from_null", result);
}

int main(void) {
    test_delete_existing();
    test_delete_first_key();
    test_delete_missing();
    test_delete_null();
    return 0;
}
