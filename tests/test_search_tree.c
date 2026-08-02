/*
 * test_search_tree.c
 *
 * Tests for: search — across a manually constructed multi-level tree
 *
 * Tree layout:
 *
 *          [ 15 ]           ← inner root (1 key)
 *         /       \
 *    [5, 10]    [15, 20]    ← leaf nodes
 *
 * Cases:
 *   search_tree_left_child      - key < split key → found in left leaf
 *   search_tree_right_child     - key >= split key → found in right leaf
 *   search_tree_split_key       - the split key itself lives in the right leaf
 *   search_tree_missing_left    - missing key that would route to left → NULL
 *   search_tree_missing_right   - missing key that would route to right → NULL
 */

#include "test_helpers.h"

static Node *build_two_level_tree(void) {
    Node *root  = create_inner_node();
    Node *left  = create_leaf_node();
    Node *right = create_leaf_node();

    /* Left leaf: keys 5, 10 */
    left->keys[0] = 5;  left->data.leaf.values[0] = (int64_t)("five");
    left->keys[1] = 10; left->data.leaf.values[1] = (int64_t)("ten");
    left->num_keys = 2;

    /* Right leaf: keys 15, 20 */
    right->keys[0] = 15; right->data.leaf.values[0] = (int64_t)("fifteen");
    right->keys[1] = 20; right->data.leaf.values[1] = (int64_t)("twenty");
    right->num_keys = 2;

    /* Inner root: split key = 15, left child, right child */
    root->keys[0] = 15;
    root->num_keys = 1;
    root->data.inner.children[0] = (left) ? (left)->page_id : INVALID_PAGE_ID;
    root->data.inner.children[1] = (right) ? (right)->page_id : INVALID_PAGE_ID;

    return root;
}

static void test_search_tree(void) {
    Node *root = build_two_level_tree();

    ASSERT_STR_EQ("search_tree_left_child",    "five",    search(root, 5));
    ASSERT_STR_EQ("search_tree_left_child_2",  "ten",     search(root, 10));
    ASSERT_STR_EQ("search_tree_right_child",   "twenty",  search(root, 20));
    ASSERT_STR_EQ("search_tree_split_key",     "fifteen", search(root, 15));

    ASSERT_NULL("search_tree_missing_left",  search(root, 7));
    ASSERT_NULL("search_tree_missing_right", search(root, 99));

    free_test_tree(root);
}

int main(void) {
    test_search_tree();
    return 0;
}
