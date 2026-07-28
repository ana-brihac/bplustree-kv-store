/*
 * test_split_leaf.c
 *
 * Tests for: split_leaf
 *
 * Cases:
 *   split_new_leaf_not_null         - split_leaf returns a non-NULL new leaf
 *   split_new_leaf_is_leaf          - the returned node is a leaf
 *   split_keys_split_evenly         - both halves get the correct number of keys
 *   split_left_keys_correct         - original leaf keeps the lower keys
 *   split_right_keys_correct        - new leaf has the upper keys
 *   split_guidepost_in_right        - smallest key of right leaf is the guidepost
 *   split_sibling_pointer_set       - original leaf's next pointer points to new leaf
 *   split_sibling_next_null         - new leaf's next pointer is NULL (end of chain)
 *   split_left_values_cleared       - moved slots are NULLed in the original leaf
 *   split_search_left_after_split   - search still finds keys in the left leaf
 *   split_search_right_after_split  - search still finds keys in the right leaf
 */

#include "test_helpers.h"

/* Build a full leaf (MAX_KEYS=4) with keys 10,20,30,40 */
static Node *build_full_leaf(void) {
	Node *leaf = create_leaf_node();
	insert_into_leaf_sorted(leaf, 10, "ten");
	insert_into_leaf_sorted(leaf, 20, "twenty");
	insert_into_leaf_sorted(leaf, 30, "thirty");
	insert_into_leaf_sorted(leaf, 40, "forty");
	return leaf;
}

static void test_split_returns_new_leaf(void) {
	Node *left = build_full_leaf();
	Node *right = split_leaf(left);

	ASSERT_NOT_NULL("split_new_leaf_not_null", right);
	ASSERT("split_new_leaf_is_leaf", right->is_leaf == true, "new node should be a leaf");

	free_test_tree(left);
	free_test_tree(right);
}

static void test_split_key_counts(void) {
	Node *left = build_full_leaf();
	int original_count = left->num_keys; // 4
	Node *right = split_leaf(left);

	/* with 4 keys, middle = 4/2 = 2 → left keeps 2, right gets 2 */
	ASSERT_INT_EQ("split_left_num_keys",  original_count / 2,             left->num_keys);
	ASSERT_INT_EQ("split_right_num_keys", original_count - original_count / 2, right->num_keys);

	free_test_tree(left);
	free_test_tree(right);
}

static void test_split_key_distribution(void) {
	Node *left = build_full_leaf();
	Node *right = split_leaf(left);

	/* left should have [10, 20], right should have [30, 40] */
	ASSERT_INT_EQ("split_left_key0",  10, left->keys[0]);
	ASSERT_INT_EQ("split_left_key1",  20, left->keys[1]);
	ASSERT_INT_EQ("split_right_key0", 30, right->keys[0]);
	ASSERT_INT_EQ("split_right_key1", 40, right->keys[1]);

	free_test_tree(left);
	free_test_tree(right);
}

static void test_split_guidepost(void) {
	Node *left = build_full_leaf();
	Node *right = split_leaf(left);

	/* guidepost is the first key of the right leaf */
	ASSERT_INT_EQ("split_guidepost_in_right", 30, right->keys[0]);

	free_test_tree(left);
	free_test_tree(right);
}

static void test_split_sibling_pointer(void) {
	Node *left = build_full_leaf();
	Node *right = split_leaf(left);

	ASSERT("split_sibling_pointer_set",  left->data.leaf.next == right, "left->next should point to right");
	ASSERT_NULL("split_sibling_next_null", right->data.leaf.next);

	free_test_tree(left);
	free_test_tree(right);
}

static void test_split_cleared_slots(void) {
	Node *left = build_full_leaf();
	Node *right = split_leaf(left);

	/* slots [2] and [3] in left should be cleared */
	ASSERT_NULL("split_left_values_cleared_2", left->data.leaf.values[2]);
	ASSERT_NULL("split_left_values_cleared_3", left->data.leaf.values[3]);

	free_test_tree(left);
	free_test_tree(right);
}

static void test_split_search_both_halves(void) {
	Node *left = build_full_leaf();
	Node *right = split_leaf(left);

	ASSERT_STR_EQ("split_search_left_10",  "ten",    search(left,  10));
	ASSERT_STR_EQ("split_search_left_20",  "twenty", search(left,  20));
	ASSERT_STR_EQ("split_search_right_30", "thirty", search(right, 30));
	ASSERT_STR_EQ("split_search_right_40", "forty",  search(right, 40));

	free_test_tree(left);
	free_test_tree(right);
}

int main(void) {
	test_split_returns_new_leaf();
	test_split_key_counts();
	test_split_key_distribution();
	test_split_guidepost();
	test_split_sibling_pointer();
	test_split_cleared_slots();
	test_split_search_both_halves();
	return 0;
}
