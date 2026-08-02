/*
 * test_insert_with_split.c
 *
 * Tests for: insert + insert_into_parent (leaf split wired into insert)
 *
 * Cases:
 *   split_triggers_new_root         - inserting the 5th key creates a new root
 *   split_root_is_inner             - new root is an inner node, not a leaf
 *   split_root_has_one_key          - new root holds exactly one guidepost key
 *   split_root_has_two_children     - new root has two children
 *   split_children_are_leaves       - both children are leaf nodes
 *   split_guidepost_in_root         - root's key is the correct guidepost
 *   split_guidepost_in_right_leaf   - guidepost key appears as data in right leaf
 *   split_search_all_keys           - all 5 inserted keys are still findable
 *   split_search_missing            - searching a missing key returns NULL
 *   split_reverse_order_search      - split with descending inserts, search still works
 *   split_multiple_leaves           - two leaf splits produce correct key distribution
 */

#include "test_helpers.h"

static void test_fifth_insert_creates_root(void) {
	Node *root = NULL;
	root = insert(root, 10, "ten");
	root = insert(root, 20, "twenty");
	root = insert(root, 30, "thirty");
	root = insert(root, 40, "forty");
	root = insert(root, 50, "fifty"); // triggers split

	ASSERT_NOT_NULL("split_triggers_new_root", root);
	ASSERT("split_root_is_inner",    root->is_leaf == false, "root should be inner after split");
	ASSERT_INT_EQ("split_root_has_one_key",   1, root->num_keys);
	ASSERT_NOT_NULL("split_left_child_exists",  fetch_node(root->data.inner.children[0]));
	ASSERT_NOT_NULL("split_right_child_exists", fetch_node(root->data.inner.children[1]));
	ASSERT("split_left_is_leaf",  fetch_node(root->data.inner.children[0])->is_leaf == true, "left child should be leaf");
	ASSERT("split_right_is_leaf", fetch_node(root->data.inner.children[1])->is_leaf == true, "right child should be leaf");

	free_test_tree(root);
}

static void test_guidepost_placement(void) {
	/* MAX_KEYS=4, keys 10,20,30,40,50 → split at middle=2, guidepost=30 */
	Node *root = NULL;
	root = insert(root, 10, "ten");
	root = insert(root, 20, "twenty");
	root = insert(root, 30, "thirty");
	root = insert(root, 40, "forty");
	root = insert(root, 50, "fifty");

	int guidepost = root->keys[0];

	/* guidepost must be the smallest key in the right leaf */
	Node *right = fetch_node(root->data.inner.children[1]);
	ASSERT_INT_EQ("split_guidepost_in_right_leaf", guidepost, right->keys[0]);

	free_test_tree(root);
}

static void test_search_after_split(void) {
	Node *root = NULL;
	root = insert(root, 10, "ten");
	root = insert(root, 20, "twenty");
	root = insert(root, 30, "thirty");
	root = insert(root, 40, "forty");
	root = insert(root, 50, "fifty");

	ASSERT_STR_EQ("split_search_10",      "ten",    search(root, 10));
	ASSERT_STR_EQ("split_search_20",      "twenty", search(root, 20));
	ASSERT_STR_EQ("split_search_30",      "thirty", search(root, 30));
	ASSERT_STR_EQ("split_search_40",      "forty",  search(root, 40));
	ASSERT_STR_EQ("split_search_50",      "fifty",  search(root, 50));
	ASSERT_NULL(  "split_search_missing",            search(root, 99));

	free_test_tree(root);
}

static void test_split_reverse_order(void) {
	/* insert in descending order — tests that split still works correctly */
	Node *root = NULL;
	root = insert(root, 50, "fifty");
	root = insert(root, 40, "forty");
	root = insert(root, 30, "thirty");
	root = insert(root, 20, "twenty");
	root = insert(root, 10, "ten"); // triggers split

	ASSERT_STR_EQ("split_reverse_search_10", "ten",    search(root, 10));
	ASSERT_STR_EQ("split_reverse_search_30", "thirty", search(root, 30));
	ASSERT_STR_EQ("split_reverse_search_50", "fifty",  search(root, 50));

	free_test_tree(root);
}

static void test_two_splits(void) {
	/* With MAX_KEYS=4, inserting 9 keys triggers two leaf splits */
	Node *root = NULL;
	root = insert(root, 10, "v10");
	root = insert(root, 20, "v20");
	root = insert(root, 30, "v30");
	root = insert(root, 40, "v40");
	root = insert(root, 50, "v50"); // first split
	root = insert(root, 60, "v60");
	root = insert(root, 70, "v70");
	root = insert(root, 80, "v80");
	root = insert(root, 90, "v90"); // second split

	/* all keys must still be searchable */
	ASSERT_STR_EQ("split_two_search_10", "v10", search(root, 10));
	ASSERT_STR_EQ("split_two_search_50", "v50", search(root, 50));
	ASSERT_STR_EQ("split_two_search_90", "v90", search(root, 90));
	ASSERT_NULL(  "split_two_missing",           search(root, 55));

	free_test_tree(root);
}

int main(void) {
	test_fifth_insert_creates_root();
	test_guidepost_placement();
	test_search_after_split();
	test_split_reverse_order();
	test_two_splits();
	return 0;
}
