/*
 * test_borrow_left.c
 *
 * Tests for: try_borrow_from_left_sibling
 *
 * All values are string literals (not heap-allocated) so we use
 * free_test_tree() at the end, not free_test_tree_with_values().
 *
 * Cases:
 *   borrow_left_leaf_returns_true        - function returns true on success
 *   borrow_left_leaf_decrements_sibling  - sibling loses one key
 *   borrow_left_leaf_increments_node     - underflowed node gains one key
 *   borrow_left_leaf_key_correct         - correct key was borrowed
 *   borrow_left_leaf_value_correct       - correct value was borrowed
 *   borrow_left_leaf_parent_guidepost    - parent separator updated to new smallest key
 *   borrow_left_leaf_sibling_stale_key   - stale last key slot in sibling is zeroed
 *   borrow_left_leaf_sibling_stale_val   - stale last value slot in sibling is NULL
 *   borrow_left_no_sibling               - index 0 -> returns false immediately
 *   borrow_left_sibling_at_minimum       - sibling at MIN keys -> returns false
 *   borrow_left_search_10                - search still works after borrow
 *   borrow_left_search_20_gone           - deleted key no longer findable
 *   borrow_left_search_30                - key that moved is still findable
 *   borrow_left_search_40                - key in right leaf still findable
 *   borrow_left_search_50                - key in right leaf still findable
 *   del_borrow_left_45_gone              - deleteNode + borrow: deleted key gone
 *   del_borrow_left_5_ok                 - other keys unaffected
 *   del_borrow_left_40_ok                - other keys unaffected
 */

#include "test_helpers.h"

/* MIN occupancy for a leaf: ceil(MAX_KEYS / 2) */
#define MIN_KEYS ((MAX_KEYS + 1) / 2)

/* ------------------------------------------------------------------
 * Build a two-leaf tree manually so we can control sibling fullness.
 *
 *  parent (inner):  key[0] = mid
 *                   children[0] = left_leaf   children[1] = right_leaf
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
    parent->data.inner.children[0] = left;
    parent->data.inner.children[1] = right;
    parent->num_keys               = 1;
    left->parent                   = parent;
    right->parent                  = parent;

    if (out_left)  *out_left  = left;
    if (out_right) *out_right = right;
    return parent;
}

/* ------------------------------------------------------------------
 * 1. Happy path: sibling has 3 keys, right leaf underflowed to 1 key
 * ------------------------------------------------------------------ */
static void test_borrow_left_leaf_happy(void) {
    int   lk[] = {10, 20, 30};
    const char *lv[] = {"ten", "twenty", "thirty"};
    int   rk[] = {50};
    const char *rv[] = {"fifty"};

    Node *left, *right;
    Node *parent = build_two_leaf_tree(lk, 3, lv, rk, 1, rv, 50,
                                       &left, &right);

    bool ok = try_borrow_from_left_sibling(right, parent, 1);

    ASSERT(       "borrow_left_leaf_returns_true",       ok,       "should return true");
    ASSERT_INT_EQ("borrow_left_leaf_decrements_sibling", 2,        left->num_keys);
    ASSERT_INT_EQ("borrow_left_leaf_increments_node",    2,        right->num_keys);
    ASSERT_INT_EQ("borrow_left_leaf_key_correct",        30,       right->keys[0]);
    ASSERT_STR_EQ("borrow_left_leaf_value_correct",      "thirty", right->data.leaf.values[0]);
    ASSERT_INT_EQ("borrow_left_leaf_parent_guidepost",   30,       parent->keys[0]);
    ASSERT_INT_EQ("borrow_left_leaf_sibling_stale_key",  0,        left->keys[2]);
    ASSERT_NULL(  "borrow_left_leaf_sibling_stale_val",            left->data.leaf.values[2]);

    free_test_tree(parent);
}

/* ------------------------------------------------------------------
 * 2. index == 0 -> no left sibling -> must return false
 * ------------------------------------------------------------------ */
static void test_borrow_left_no_sibling(void) {
    int   lk[] = {10, 20, 30};
    const char *lv[] = {"ten", "twenty", "thirty"};
    int   rk[] = {50};
    const char *rv[] = {"fifty"};

    Node *left, *right;
    Node *parent = build_two_leaf_tree(lk, 3, lv, rk, 1, rv, 50,
                                       &left, &right);

    bool ok = try_borrow_from_left_sibling(left, parent, 0);

    ASSERT("borrow_left_no_sibling", !ok, "should return false when index==0");

    free_test_tree(parent);
}

/* ------------------------------------------------------------------
 * 3. Sibling at exactly MIN_KEYS -> cannot lend -> return false
 * ------------------------------------------------------------------ */
static void test_borrow_left_sibling_at_minimum(void) {
    /* MIN_KEYS = ceil(4/2) = 2 */
    int   lk[] = {10, 20};
    const char *lv[] = {"ten", "twenty"};
    int   rk[] = {50};
    const char *rv[] = {"fifty"};

    Node *left, *right;
    Node *parent = build_two_leaf_tree(lk, 2, lv, rk, 1, rv, 50,
                                       &left, &right);

    bool ok = try_borrow_from_left_sibling(right, parent, 1);

    ASSERT("borrow_left_sibling_at_minimum", !ok,
           "should return false when sibling is at minimum occupancy");

    free_test_tree(parent);
}

/* ------------------------------------------------------------------
 * 4. Full end-to-end: insert 5 keys, delete one from left leaf,
 *    which triggers borrow-right-from-right (or borrow-left from right's
 *    perspective). We test search after deleteNode handles the borrow.
 *
 *  With MAX_KEYS=4, inserting 10..50 gives:
 *    root [30] -> left[10,20]  right[30,40,50]
 *  Deleting 20 -> left underflows -> borrows 30 from right
 *    root [40] -> left[10,30]  right[40,50]
 * ------------------------------------------------------------------ */
static void test_borrow_left_search_after_borrow(void) {
    Node *root = NULL;
    root = insert(root, 10, strdup("ten"));
    root = insert(root, 20, strdup("twenty"));
    root = insert(root, 30, strdup("thirty"));
    root = insert(root, 40, strdup("forty"));
    root = insert(root, 50, strdup("fifty"));

    root = deleteNode(root, 20);

    ASSERT_STR_EQ("borrow_left_search_10",    "ten",    search(root, 10));
    ASSERT_NULL(  "borrow_left_search_20_gone",          search(root, 20));
    ASSERT_STR_EQ("borrow_left_search_30",    "thirty", search(root, 30));
    ASSERT_STR_EQ("borrow_left_search_40",    "forty",  search(root, 40));
    ASSERT_STR_EQ("borrow_left_search_50",    "fifty",  search(root, 50));

    free_test_tree_with_values(root);
}

/* ------------------------------------------------------------------
 * 5. deleteNode triggers borrow-left on a bigger tree
 * ------------------------------------------------------------------ */
static void test_delete_triggers_borrow_left(void) {
    Node *root = NULL;
    root = insert(root, 5,  strdup("five"));
    root = insert(root, 10, strdup("ten"));
    root = insert(root, 15, strdup("fifteen"));
    root = insert(root, 20, strdup("twenty"));
    root = insert(root, 25, strdup("twenty-five"));
    root = insert(root, 30, strdup("thirty"));
    root = insert(root, 35, strdup("thirty-five"));
    root = insert(root, 40, strdup("forty"));
    root = insert(root, 45, strdup("forty-five"));

    root = deleteNode(root, 45);

    ASSERT_NULL(  "del_borrow_left_45_gone", search(root, 45));
    ASSERT_STR_EQ("del_borrow_left_5_ok",   "five",  search(root, 5));
    ASSERT_STR_EQ("del_borrow_left_40_ok",  "forty", search(root, 40));

    free_test_tree_with_values(root);
}

int main(void) {
    test_borrow_left_leaf_happy();
    test_borrow_left_no_sibling();
    test_borrow_left_sibling_at_minimum();
    test_borrow_left_search_after_borrow();
    test_delete_triggers_borrow_left();
    return 0;
}
