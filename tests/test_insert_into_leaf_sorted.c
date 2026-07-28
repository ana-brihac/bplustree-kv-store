/*
 * test_insert_into_leaf_sorted.c
 *
 * Tests for: insert_into_leaf_sorted
 *
 * Cases:
 *   sorted_single_key              - one key inserted, num_keys is 1
 *   sorted_key_stored_correctly    - key is stored at index 0
 *   sorted_value_stored_correctly  - value is stored at index 0
 *   sorted_ascending_order         - keys stay sorted after out-of-order inserts
 *   sorted_values_follow_keys      - values follow their keys after reordering
 *   sorted_prepend_key             - inserting smallest key shifts others right
 *   sorted_middle_key              - inserting middle key shifts only right side
 *   sorted_duplicate_rejected      - duplicate key does not increase num_keys
 *   sorted_duplicate_value_kept    - original value is kept on duplicate insert
 *   sorted_four_keys_full          - filling to MAX_KEYS works correctly
 */

#include "test_helpers.h"

static void test_single_insert(void) {
	Node *leaf = create_leaf_node();
	insert_into_leaf_sorted(leaf, 42, "forty-two");

	ASSERT_INT_EQ("sorted_single_key",             1,          leaf->num_keys);
	ASSERT_INT_EQ("sorted_key_stored_correctly",   42,         leaf->keys[0]);
	ASSERT_STR_EQ("sorted_value_stored_correctly", "forty-two", leaf->data.leaf.values[0]);

	free_test_tree(leaf);
}

static void test_out_of_order_inserts(void) {
	Node *leaf = create_leaf_node();
	insert_into_leaf_sorted(leaf, 30, "thirty");
	insert_into_leaf_sorted(leaf, 10, "ten");
	insert_into_leaf_sorted(leaf, 20, "twenty");

	/* keys must be [10, 20, 30] */
	ASSERT_INT_EQ("sorted_ascending_key0", 10, leaf->keys[0]);
	ASSERT_INT_EQ("sorted_ascending_key1", 20, leaf->keys[1]);
	ASSERT_INT_EQ("sorted_ascending_key2", 30, leaf->keys[2]);

	/* values must follow their keys */
	ASSERT_STR_EQ("sorted_values_follow_key0", "ten",    leaf->data.leaf.values[0]);
	ASSERT_STR_EQ("sorted_values_follow_key1", "twenty", leaf->data.leaf.values[1]);
	ASSERT_STR_EQ("sorted_values_follow_key2", "thirty", leaf->data.leaf.values[2]);

	free_test_tree(leaf);
}

static void test_prepend(void) {
	Node *leaf = create_leaf_node();
	insert_into_leaf_sorted(leaf, 20, "twenty");
	insert_into_leaf_sorted(leaf, 30, "thirty");
	insert_into_leaf_sorted(leaf, 5,  "five"); // should go to index 0

	ASSERT_INT_EQ("sorted_prepend_key",  5, leaf->keys[0]);
	ASSERT_INT_EQ("sorted_prepend_key1", 20, leaf->keys[1]);
	ASSERT_INT_EQ("sorted_prepend_key2", 30, leaf->keys[2]);

	free_test_tree(leaf);
}

static void test_middle_insert(void) {
	Node *leaf = create_leaf_node();
	insert_into_leaf_sorted(leaf, 10, "ten");
	insert_into_leaf_sorted(leaf, 30, "thirty");
	insert_into_leaf_sorted(leaf, 20, "twenty"); // goes between 10 and 30

	ASSERT_INT_EQ("sorted_middle_key0", 10, leaf->keys[0]);
	ASSERT_INT_EQ("sorted_middle_key1", 20, leaf->keys[1]);
	ASSERT_INT_EQ("sorted_middle_key2", 30, leaf->keys[2]);

	free_test_tree(leaf);
}

static void test_duplicate_rejected(void) {
	Node *leaf = create_leaf_node();
	insert_into_leaf_sorted(leaf, 10, "original");
	insert_into_leaf_sorted(leaf, 10, "duplicate");

	ASSERT_INT_EQ("sorted_duplicate_rejected",   1,          leaf->num_keys);
	ASSERT_STR_EQ("sorted_duplicate_value_kept", "original", leaf->data.leaf.values[0]);

	free_test_tree(leaf);
}

static void test_fill_to_max(void) {
	Node *leaf = create_leaf_node();
	insert_into_leaf_sorted(leaf, 1,  "one");
	insert_into_leaf_sorted(leaf, 2,  "two");
	insert_into_leaf_sorted(leaf, 3,  "three");
	insert_into_leaf_sorted(leaf, 4,  "four");

	ASSERT_INT_EQ("sorted_four_keys_full",   MAX_KEYS, leaf->num_keys);
	ASSERT_INT_EQ("sorted_four_keys_key3",   4,        leaf->keys[3]);
	ASSERT_STR_EQ("sorted_four_keys_value3", "four",   leaf->data.leaf.values[3]);

	free_test_tree(leaf);
}

int main(void) {
	test_single_insert();
	test_out_of_order_inserts();
	test_prepend();
	test_middle_insert();
	test_duplicate_rejected();
	test_fill_to_max();
	return 0;
}
