/*
 * test_remove_from_leaf.c
 *
 * Tests for: remove_from_leaf
 *
 * Note: remove_from_leaf calls free() on the removed value, so all values
 * here are heap-allocated with strdup().
 *
 * Cases:
 *   remove_decrements_num_keys      - num_keys goes from 3 to 2 after removal
 *   remove_key_no_longer_found      - search returns NULL for the removed key
 *   remove_others_remain            - other keys are still searchable
 *   remove_shifts_keys_left         - key array shifts correctly after removal
 *   remove_clears_stale_slot        - the vacated last slot is zeroed/NULLed
 *   remove_first_key                - removing index 0 shifts all others left
 *   remove_last_key                 - removing the last key leaves the rest intact
 *   remove_missing_key_unchanged    - num_keys unchanged when key is not present
 *   remove_from_single_key_leaf     - removing the only key leaves num_keys=0
 *   remove_returns_leaf             - remove_from_leaf returns the same leaf pointer
 */

#include "test_helpers.h"

static Node *build_heap_leaf(void) {
	Node *leaf = create_leaf_node();
	insert_into_leaf_sorted(leaf, 10, strdup("ten"));
	insert_into_leaf_sorted(leaf, 20, strdup("twenty"));
	insert_into_leaf_sorted(leaf, 30, strdup("thirty"));
	return leaf;
}

static void test_remove_middle(void) {
	Node *leaf = build_heap_leaf();

	remove_from_leaf(leaf, 20);

	ASSERT_INT_EQ("remove_decrements_num_keys",  2, leaf->num_keys);
	ASSERT_NULL(  "remove_key_no_longer_found",     search(leaf, 20));
	ASSERT_STR_EQ("remove_others_remain_10", "ten",    search(leaf, 10));
	ASSERT_STR_EQ("remove_others_remain_30", "thirty", search(leaf, 30));

	/* keys should be [10, 30] */
	ASSERT_INT_EQ("remove_shifts_keys_key0", 10, leaf->keys[0]);
	ASSERT_INT_EQ("remove_shifts_keys_key1", 30, leaf->keys[1]);

	/* stale slot [2] should be cleared */
	ASSERT_INT_EQ("remove_clears_stale_key",   0,    leaf->keys[2]);
	ASSERT_NULL(  "remove_clears_stale_value",        leaf->data.leaf.values[2]);

	free_test_tree_with_values(leaf);
}

static void test_remove_first(void) {
	Node *leaf = build_heap_leaf();

	remove_from_leaf(leaf, 10);

	ASSERT_INT_EQ("remove_first_num_keys",  2, leaf->num_keys);
	ASSERT_NULL(  "remove_first_key_gone",     search(leaf, 10));
	ASSERT_INT_EQ("remove_first_shifts_key0", 20, leaf->keys[0]);
	ASSERT_INT_EQ("remove_first_shifts_key1", 30, leaf->keys[1]);

	free_test_tree_with_values(leaf);
}

static void test_remove_last(void) {
	Node *leaf = build_heap_leaf();

	remove_from_leaf(leaf, 30);

	ASSERT_INT_EQ("remove_last_num_keys",  2, leaf->num_keys);
	ASSERT_NULL(  "remove_last_key_gone",     search(leaf, 30));
	ASSERT_STR_EQ("remove_last_others_10", "ten",    search(leaf, 10));
	ASSERT_STR_EQ("remove_last_others_20", "twenty", search(leaf, 20));

	free_test_tree_with_values(leaf);
}

static void test_remove_missing(void) {
	Node *leaf = build_heap_leaf();

	remove_from_leaf(leaf, 999); // key not present

	ASSERT_INT_EQ("remove_missing_key_unchanged", 3, leaf->num_keys);

	free_test_tree_with_values(leaf);
}

static void test_remove_only_key(void) {
	Node *leaf = create_leaf_node();
	insert_into_leaf_sorted(leaf, 42, strdup("only"));

	remove_from_leaf(leaf, 42);

	ASSERT_INT_EQ("remove_single_key_num_keys_zero", 0, leaf->num_keys);
	ASSERT_NULL(  "remove_single_key_gone",             search(leaf, 42));

	free(leaf); // no values left to free
}

static void test_remove_returns_leaf(void) {
	Node *leaf = build_heap_leaf();
	Node *result = remove_from_leaf(leaf, 10);

	ASSERT("remove_returns_leaf", result == leaf, "should return the same leaf pointer");

	free_test_tree_with_values(leaf);
}

int main(void) {
	test_remove_middle();
	test_remove_first();
	test_remove_last();
	test_remove_missing();
	test_remove_only_key();
	test_remove_returns_leaf();
	return 0;
}
