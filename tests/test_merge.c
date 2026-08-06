/*
 * test_merge.c
 *
 * Tests for Phase 7: merge_with_sibling, cascading merges, and root shrinking.
 *
 * Values are heap-allocated with strdup() when deleteNode is called, as
 * it eventually calls remove_from_leaf which calls free().
 */

#include "test_helpers.h"

/* Helper to build a simple two-leaf tree for isolated merge testing */
static Node *build_mergeable_tree(
        int *left_keys,  int left_count,  const char **left_vals,
        int *right_keys, int right_count, const char **right_vals,
        int mid, Node **out_left, Node **out_right)
{
    Node *parent = create_inner_node();
    Node *left   = create_leaf_node();
    Node *right  = create_leaf_node();

    for (int i = 0; i < left_count; i++)
        insert_into_leaf_sorted(left, left_keys[i], (void *)left_vals[i]);
    for (int i = 0; i < right_count; i++)
        insert_into_leaf_sorted(right, right_keys[i], (void *)right_vals[i]);

    parent->keys[0]                = mid;
    parent->data.inner.children[0] = (left) ? (left)->page_id : INVALID_PAGE_ID;
    parent->data.inner.children[1] = (right) ? (right)->page_id : INVALID_PAGE_ID;
    parent->num_keys               = 1;
    left->parent_id = (parent) ? (parent)->page_id : INVALID_PAGE_ID;
    right->parent_id = (parent) ? (parent)->page_id : INVALID_PAGE_ID;

    if (out_left)  *out_left  = left;
    if (out_right) *out_right = right;
    return parent;
}

static void test_merge_leaf_right(void) {
    int   lk[] = {10, 20};
    const char *lv[] = {"ten", "twenty"};
    int   rk[] = {30};
    const char *rv[] = {"thirty"};

    Node *left, *right;
    Node *parent = build_mergeable_tree(lk, 2, lv, rk, 1, rv, 30, &left, &right);

    // merge left with right sibling
    Node *res = merge_with_sibling(left, right, parent, 0);

    ASSERT("merge_leaf_right_returns_parent", res == parent, "merge should return parent");
    ASSERT_INT_EQ("merge_leaf_right_num_keys", 3, left->num_keys);
    ASSERT_INT_EQ("merge_leaf_right_parent_keys", 0, parent->num_keys);
    ASSERT_STR_EQ("merge_leaf_right_val_0", "ten", left->data.leaf.values[0]);
    ASSERT_STR_EQ("merge_leaf_right_val_2", "thirty", left->data.leaf.values[2]);

    free_test_tree(parent);
}

static void test_delete_triggers_merge(void) {
    Node *root = NULL;
    root = insert(root, 10, strdup("ten"));
    root = insert(root, 20, strdup("twenty"));
    root = insert(root, 30, strdup("thirty"));
    root = insert(root, 40, strdup("forty"));
    root = insert(root, 50, strdup("fifty"));

    // deleting 50 makes right leaf underflow (1 key: 40). 
    // left leaf has [10, 20]. Borrow fails, so they must merge.
    root = deleteNode(root, 50);

    ASSERT_NULL("merge_trigger_50_gone", search(root, 50));
    ASSERT_STR_EQ("merge_trigger_10_ok", "ten", search(root, 10));
    ASSERT_STR_EQ("merge_trigger_40_ok", "forty", search(root, 40));

    free_test_tree_with_values(root);
}

static void test_root_shrinking(void) {
    Node *root = NULL;
    root = insert(root, 10, strdup("10"));
    root = insert(root, 20, strdup("20"));
    root = insert(root, 30, strdup("30"));
    root = insert(root, 40, strdup("40"));
    root = insert(root, 50, strdup("50"));
    
    // root is an inner node with 2 children.
    ASSERT("root_is_inner_before", !root->is_leaf, "root should be inner before merge");

    // deleting 50 and 40 causes a merge. 
    // the two leaf children merge into one leaf.
    // root (inner) drops to 0 keys (1 child).
    // root should shrink, becoming a leaf.
    root = deleteNode(root, 50);
    root = deleteNode(root, 40);

    ASSERT("root_is_leaf_after", root->is_leaf, "root should shrink to a leaf after merge");
    ASSERT_INT_EQ("root_keys_after", 3, root->num_keys);

    free_test_tree_with_values(root);
}

static void test_cascading_merge(void) {
    // We need enough keys to create a 3-level tree: root -> inner -> leaf
    Node *root = NULL;
    for (int i = 1; i <= 17; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "v%d", i);
        root = insert(root, i * 10, strdup(buf));
    }

    // Now delete keys to trigger a chain of underflows leading to a cascading merge.
    root = deleteNode(root, 170);
    root = deleteNode(root, 160);
    root = deleteNode(root, 150);
    root = deleteNode(root, 140);
    root = deleteNode(root, 130);
    root = deleteNode(root, 120);
    root = deleteNode(root, 110);
    
    ASSERT_NULL("cascading_merge_170_gone", search(root, 170));
    ASSERT_STR_EQ("cascading_merge_10_ok", "v1", search(root, 10));
    ASSERT_STR_EQ("cascading_merge_100_ok", "v10", search(root, 100));

    free_test_tree_with_values(root);
}

int main(void) {
    test_merge_leaf_right();
    test_delete_triggers_merge();
    test_root_shrinking();
    test_cascading_merge();
    return 0;
}
