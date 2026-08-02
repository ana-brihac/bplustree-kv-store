/*
 * test_borrow_right.c
 *
 * Tests for: try_borrow_from_right_sibling
 *
 * All values are string literals (not heap-allocated) so we use
 * free_test_tree() at the end, not free_test_tree_with_values().
 *
 * Cases:
 *   borrow_right_leaf_returns_true       - function returns true on success
 *   borrow_right_leaf_decrements_sibling - sibling loses one key
 *   borrow_right_leaf_increments_node    - underflowed node gains one key
 *   borrow_right_leaf_key_correct        - correct key was borrowed
 *   borrow_right_leaf_value_correct      - correct value was borrowed
 *   borrow_right_leaf_parent_guidepost   - parent separator updated to new smallest sibling key
 *   borrow_right_leaf_sibling_stale_key  - stale last key slot in sibling is zeroed
 *   borrow_right_leaf_sibling_stale_val  - stale last value slot in sibling is NULL
 *   borrow_right_no_sibling              - index == parent->num_keys -> returns false
 *   borrow_right_sibling_at_minimum      - sibling at MIN keys -> returns false
 *   borrow_right_search_10               - search still works after borrow
 *   borrow_right_search_20_gone          - deleted key no longer findable
 *   borrow_right_search_30               - key that moved is still findable
 *   borrow_right_search_40               - key in right leaf still findable
 *   borrow_right_search_50               - key in right leaf still findable
 *   del_borrow_right_10_gone             - deleteNode + borrow: deleted key gone
 *   del_borrow_right_20_ok               - other keys unaffected
 *   del_borrow_right_50_ok               - other keys unaffected
 */

#include "test_helpers.h"

#define MIN_KEYS ((MAX_KEYS + 1) / 2)

/* ------------------------------------------------------------------
 * Shared builder: two-leaf tree with controllable occupancy
 * ------------------------------------------------------------------ */
static Node *build_two_leaf_tree(
        int *left_keys,  int left_count,  const char **left_vals,
        int *right_keys, int right_count, const char **right_vals,
        int mid,
        Node **out_left, Node **out_right)
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

/* ------------------------------------------------------------------
 * 1. Happy path: left leaf underflowed to 1 key, right sibling has 3
 * ------------------------------------------------------------------ */
static void test_borrow_right_leaf_happy(void) {
    int   lk[] = {10};
    const char *lv[] = {"ten"};
    int   rk[] = {30, 40, 50};
    const char *rv[] = {"thirty", "forty", "fifty"};

    Node *left, *right;
    /* mid separator = 30 (smallest key in right leaf) */
    Node *parent = build_two_leaf_tree(lk, 1, lv, rk, 3, rv, 30,
                                       &left, &right);

    bool ok = try_borrow_from_right_sibling(left, parent, 0);

    ASSERT(       "borrow_right_leaf_returns_true",       ok,        "should return true");
    ASSERT_INT_EQ("borrow_right_leaf_increments_node",    2,         left->num_keys);
    ASSERT_INT_EQ("borrow_right_leaf_decrements_sibling", 2,         right->num_keys);
    ASSERT_INT_EQ("borrow_right_leaf_key_correct",        30,        left->keys[1]);
    ASSERT_STR_EQ("borrow_right_leaf_value_correct",      "thirty",  left->data.leaf.values[1]);
    /* parent separator must now be the new smallest key in right sibling */
    ASSERT_INT_EQ("borrow_right_leaf_parent_guidepost",   40,        parent->keys[0]);
    /* stale last slot in right sibling must be cleared */
    ASSERT_INT_EQ("borrow_right_leaf_sibling_stale_key",  0,         right->keys[2]);
    ASSERT_NULL(  "borrow_right_leaf_sibling_stale_val",             right->data.leaf.values[2]);

    free_test_tree(parent);
}

/* ------------------------------------------------------------------
 * 2. index == parent->num_keys -> no right sibling -> return false
 * ------------------------------------------------------------------ */
static void test_borrow_right_no_sibling(void) {
    int   lk[] = {10};
    const char *lv[] = {"ten"};
    int   rk[] = {30, 40, 50};
    const char *rv[] = {"thirty", "forty", "fifty"};

    Node *left, *right;
    Node *parent = build_two_leaf_tree(lk, 1, lv, rk, 3, rv, 30,
                                       &left, &right);

    /* index=1 == parent->num_keys=1 -> no right sibling */
    bool ok = try_borrow_from_right_sibling(right, parent, 1);

    ASSERT("borrow_right_no_sibling", !ok,
           "should return false when no right sibling exists");

    free_test_tree(parent);
}

/* ------------------------------------------------------------------
 * 3. Right sibling at exactly MIN_KEYS -> cannot lend -> return false
 * ------------------------------------------------------------------ */
static void test_borrow_right_sibling_at_minimum(void) {
    int   lk[] = {10};
    const char *lv[] = {"ten"};
    int   rk[] = {30, 40};          /* exactly MIN_KEYS = 2 */
    const char *rv[] = {"thirty", "forty"};

    Node *left, *right;
    Node *parent = build_two_leaf_tree(lk, 1, lv, rk, 2, rv, 30,
                                       &left, &right);

    bool ok = try_borrow_from_right_sibling(left, parent, 0);

    ASSERT("borrow_right_sibling_at_minimum", !ok,
           "should return false when sibling is at minimum occupancy");

    free_test_tree(parent);
}

/* ------------------------------------------------------------------
 * 4. End-to-end: insert 5 keys, delete from right leaf, triggers
 *    borrow from right sibling on the left leaf.
 *
 *  With MAX_KEYS=4, inserting 10..50:
 *    root [30] -> left[10,20]  right[30,40,50]
 *  Deleting 10 -> left underflows (1 key) -> borrows 30 from right
 *    root [40] -> left[10... wait, left now has [20,30]  right[40,50]
 *
 * Actually: after deleting 10, left=[20] (underflow).
 * try_borrow_from_right_sibling: left borrows 30 from right.
 * left=[20,30], right=[40,50], parent sep=40.
 * ------------------------------------------------------------------ */
static void test_borrow_right_search_after_borrow(void) {
    Node *root = NULL;
    root = insert(root, 10, strdup("ten"));
    root = insert(root, 20, strdup("twenty"));
    root = insert(root, 30, strdup("thirty"));
    root = insert(root, 40, strdup("forty"));
    root = insert(root, 50, strdup("fifty"));

    root = deleteNode(root, 10);

    ASSERT_NULL(  "borrow_right_search_10_gone",           search(root, 10));
    ASSERT_STR_EQ("borrow_right_search_20",   "twenty",   search(root, 20));
    ASSERT_STR_EQ("borrow_right_search_30",   "thirty",   search(root, 30));
    ASSERT_STR_EQ("borrow_right_search_40",   "forty",    search(root, 40));
    ASSERT_STR_EQ("borrow_right_search_50",   "fifty",    search(root, 50));

    free_test_tree_with_values(root);
}

/* ------------------------------------------------------------------
 * 5. deleteNode triggers borrow-right on a bigger tree
 * ------------------------------------------------------------------ */
static void test_delete_triggers_borrow_right(void) {
    Node *root = NULL;
    root = insert(root, 10, strdup("ten"));
    root = insert(root, 20, strdup("twenty"));
    root = insert(root, 30, strdup("thirty"));
    root = insert(root, 40, strdup("forty"));
    root = insert(root, 50, strdup("fifty"));
    root = insert(root, 60, strdup("sixty"));
    root = insert(root, 70, strdup("seventy"));
    root = insert(root, 80, strdup("eighty"));
    root = insert(root, 90, strdup("ninety"));

    root = deleteNode(root, 10);

    ASSERT_NULL(  "del_borrow_right_10_gone", search(root, 10));
    ASSERT_STR_EQ("del_borrow_right_20_ok",  "twenty",  search(root, 20));
    ASSERT_STR_EQ("del_borrow_right_50_ok",  "fifty",   search(root, 50));

    free_test_tree_with_values(root);
}

int main(void) {
    test_borrow_right_leaf_happy();
    test_borrow_right_no_sibling();
    test_borrow_right_sibling_at_minimum();
    test_borrow_right_search_after_borrow();
    test_delete_triggers_borrow_right();
    return 0;
}
