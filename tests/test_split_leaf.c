void *dummy_raw;
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
#include "../src/buffer_pool.h"
#include "../src/page_manager.h"
#include "../src/serialize.h"

/* Build a full leaf (MAX_KEYS=4) with keys 10,20,30,40 */
static page_id_t build_full_leaf(BufferPool *bp) {
    page_id_t leaf_id = create_leaf_node(bp);
    void *raw = bp_fetch_page(bp, leaf_id);
    Node *leaf = deserialize_node(raw);
    insert_into_leaf_sorted(leaf, 10, "ten");
    insert_into_leaf_sorted(leaf, 20, "twenty");
    insert_into_leaf_sorted(leaf, 30, "thirty");
    insert_into_leaf_sorted(leaf, 40, "forty");
    serialize_node(leaf, raw);
    bp_unpin(bp, leaf_id, true);
    free(leaf);
    return leaf_id;
}

static void test_split_returns_new_leaf(void) {
    remove("test_split_leaf1.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_split_leaf1.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t left_id = build_full_leaf(bp);
    tree->root_id = left_id;
    
    void *l_raw = bp_fetch_page(bp, left_id);
    Node *left = deserialize_node(l_raw);
    
    Node *right = split_leaf(bp, left, &dummy_raw);

    ASSERT_NOT_NULL("split_new_leaf_not_null", right);
    ASSERT("split_new_leaf_is_leaf", right->is_leaf == true, "new node should be a leaf");

    bp_unpin(bp, left_id, false);
    free(left);
    free(right); // split_leaf returns a heap-allocated deserialized node
    
    tree_close(tree);
        remove("test_split_leaf1.db");
}

static void test_split_key_counts(void) {
    remove("test_split_leaf2.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_split_leaf2.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t left_id = build_full_leaf(bp);
    tree->root_id = left_id;
    void *l_raw = bp_fetch_page(bp, left_id);
    Node *left = deserialize_node(l_raw);
    
    int original_count = left->num_keys;
    Node *right = split_leaf(bp, left, &dummy_raw);

    ASSERT_INT_EQ("split_left_num_keys",  original_count / 2,             left->num_keys);
    ASSERT_INT_EQ("split_right_num_keys", original_count - original_count / 2, right->num_keys);

    bp_unpin(bp, left_id, false);
    free(left);
    free(right);
    tree_close(tree);
        remove("test_split_leaf2.db");
}

static void test_split_key_distribution(void) {
    remove("test_split_leaf3.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_split_leaf3.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t left_id = build_full_leaf(bp);
    tree->root_id = left_id;
    void *l_raw = bp_fetch_page(bp, left_id);
    Node *left = deserialize_node(l_raw);
    Node *right = split_leaf(bp, left, &dummy_raw);

    ASSERT_INT_EQ("split_left_key0",  10, left->keys[0]);
    ASSERT_INT_EQ("split_left_key1",  20, left->keys[1]);
    ASSERT_INT_EQ("split_right_key0", 30, right->keys[0]);
    ASSERT_INT_EQ("split_right_key1", 40, right->keys[1]);

    bp_unpin(bp, left_id, false);
    free(left);
    free(right);
    tree_close(tree);
        remove("test_split_leaf3.db");
}

static void test_split_guidepost(void) {
    remove("test_split_leaf4.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_split_leaf4.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t left_id = build_full_leaf(bp);
    tree->root_id = left_id;
    void *l_raw = bp_fetch_page(bp, left_id);
    Node *left = deserialize_node(l_raw);
    Node *right = split_leaf(bp, left, &dummy_raw);

    ASSERT_INT_EQ("split_guidepost_in_right", 30, right->keys[0]);

    bp_unpin(bp, left_id, false);
    free(left);
    free(right);
    tree_close(tree);
        remove("test_split_leaf4.db");
}

static void test_split_sibling_pointer(void) {
    remove("test_split_leaf5.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_split_leaf5.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t left_id = build_full_leaf(bp);
    tree->root_id = left_id;
    void *l_raw = bp_fetch_page(bp, left_id);
    Node *left = deserialize_node(l_raw);
    Node *right = split_leaf(bp, left, &dummy_raw);

    ASSERT("split_sibling_pointer_set",  left->data.leaf.next_id == right->page_id, "left->next should point to right");
    ASSERT("split_sibling_next_null", right->data.leaf.next_id == INVALID_PAGE_ID, "right->next should be INVALID_PAGE_ID");

    bp_unpin(bp, left_id, false);
    free(left);
    free(right);
    tree_close(tree);
        remove("test_split_leaf5.db");
}

static void test_split_cleared_slots(void) {
    remove("test_split_leaf6.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_split_leaf6.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t left_id = build_full_leaf(bp);
    tree->root_id = left_id;
    void *l_raw = bp_fetch_page(bp, left_id);
    Node *left = deserialize_node(l_raw);
    Node *right = split_leaf(bp, left, &dummy_raw);

    ASSERT("split_left_values_cleared_2", left->data.leaf.values[2] == 0, "should be 0");
    ASSERT("split_left_values_cleared_3", left->data.leaf.values[3] == 0, "should be 0");

    bp_unpin(bp, left_id, false);
    free(left);
    free(right);
    tree_close(tree);
        remove("test_split_leaf6.db");
}

static void test_split_search_both_halves(void) {
    remove("test_split_leaf7.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_split_leaf7.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t left_id = build_full_leaf(bp);
    tree->root_id = left_id;
    void *l_raw = bp_fetch_page(bp, left_id);
    Node *left = deserialize_node(l_raw);
    Node *right = split_leaf(bp, left, &dummy_raw);
    
    // In our disk-backed system, search takes (BufferPool, page_id_t, key)
    // Here left and right are nodes in memory, search won't work on them directly
    // since search fetches from bp. So let's serialize them, then run search on their page_ids.
    serialize_node(left, l_raw);
    bp_unpin(bp, left_id, true);
    
    page_id_t right_id = right->page_id;
    // Note: split_leaf already fetched and deserialized new_leaf, but we must serialize it manually
    void *r_raw = bp_fetch_page(bp, right_id);
    serialize_node(right, r_raw);
    bp_unpin(bp, right_id, true);
    
    page_id_t right_id_captured = right->page_id;
    free(left);
    free(right);

    ASSERT_STR_EQ("split_search_left_10",  "ten",    (char*)search(tree,  10));
    ASSERT_STR_EQ("split_search_left_20",  "twenty", (char*)search(tree,  20));
    
    tree->root_id = right_id_captured;
    ASSERT_STR_EQ("split_search_right_30", "thirty", (char*)search(tree, 30));
    ASSERT_STR_EQ("split_search_right_40", "forty",  (char*)search(tree, 40));

    tree_close(tree);
        remove("test_split_leaf7.db");
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
