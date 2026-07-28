/*
 * test_insert_basic.c
 *
 * Tests for: insert — basic behavior (empty tree, sort order, duplicates)
 *
 * Cases:
 *   insert_empty_returns_non_null    - inserting into NULL returns a valid node
 *   insert_empty_is_leaf             - first node is a leaf
 *   insert_empty_num_keys_one        - one key after first insert
 *   insert_empty_key_stored          - the correct key is stored at index 0
 *   insert_empty_value_stored        - the correct value is stored at index 0
 *   insert_sort_num_keys             - three inserts → three keys
 *   insert_sort_order_keys           - keys are sorted ascending after inserts
 *   insert_sort_values_follow        - values follow their keys after sorting
 *   insert_duplicate_num_keys        - duplicate key does not increase num_keys
 *   insert_duplicate_value_unchanged - duplicate key does not overwrite existing value
 */

#include "test_helpers.h"

static void test_insert_empty(void) {
    Node *root = NULL;
    root = insert(root, 10, "ten");

    ASSERT_NOT_NULL("insert_empty_returns_non_null", root);
    ASSERT("insert_empty_is_leaf",   root->is_leaf == true, "should be a leaf");
    ASSERT_INT_EQ("insert_empty_num_keys_one", 1, root->num_keys);
    ASSERT_INT_EQ("insert_empty_key_stored",   10, root->keys[0]);
    ASSERT_STR_EQ("insert_empty_value_stored", "ten", root->data.leaf.values[0]);

    free_test_tree(root);
}

static void test_insert_sort_order(void) {
    Node *root = NULL;
    root = insert(root, 20, "twenty");
    root = insert(root, 5,  "five");
    root = insert(root, 10, "ten");

    ASSERT_INT_EQ("insert_sort_num_keys", 3, root->num_keys);

    /* keys should be [5, 10, 20] */
    ASSERT_INT_EQ("insert_sort_order_key0",  5,  root->keys[0]);
    ASSERT_INT_EQ("insert_sort_order_key1", 10,  root->keys[1]);
    ASSERT_INT_EQ("insert_sort_order_key2", 20,  root->keys[2]);

    /* values follow their keys */
    ASSERT_STR_EQ("insert_sort_value0", "five",   root->data.leaf.values[0]);
    ASSERT_STR_EQ("insert_sort_value1", "ten",    root->data.leaf.values[1]);
    ASSERT_STR_EQ("insert_sort_value2", "twenty", root->data.leaf.values[2]);

    free_test_tree(root);
}

static void test_insert_duplicate(void) {
    Node *root = NULL;
    root = insert(root, 10, "ten");
    root = insert(root, 10, "duplicate");

    ASSERT_INT_EQ("insert_duplicate_num_keys",         1,   root->num_keys);
    ASSERT_STR_EQ("insert_duplicate_value_unchanged", "ten", root->data.leaf.values[0]);

    free_test_tree(root);
}

int main(void) {
    test_insert_empty();
    test_insert_sort_order();
    test_insert_duplicate();
    return 0;
}
