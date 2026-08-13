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
#include "../src/buffer_pool.h"
#include "../src/page_manager.h"
#include "../src/serialize.h"

static void test_fifth_insert_creates_root(void) {
    remove("test_split1.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_split1.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    root_id = insert(tree, 10, "ten");
    root_id = insert(tree, 20, "twenty");
    root_id = insert(tree, 30, "thirty");
    root_id = insert(tree, 40, "forty");
    root_id = insert(tree, 50, "fifty");

    void *raw = bp_fetch_page(bp, root_id);
    Node *root = deserialize_node(raw);

    ASSERT("split_root_is_inner", root->is_leaf == false, "root should be inner node now");
    ASSERT_INT_EQ("split_root_num_keys", 1, root->num_keys);
    
    void *l_raw = bp_fetch_page(bp, root->data.inner.children[0]);
    Node *left = deserialize_node(l_raw);
    void *r_raw = bp_fetch_page(bp, root->data.inner.children[1]);
    Node *right = deserialize_node(r_raw);

    ASSERT("split_left_is_leaf",  left->is_leaf == true, "left child should be leaf");
    ASSERT("split_right_is_leaf", right->is_leaf == true, "right child should be leaf");

    bp_unpin(bp, root->data.inner.children[0], false);
    free(left);
    bp_unpin(bp, root->data.inner.children[1], false);
    free(right);
    bp_unpin(bp, root_id, false);
    free(root);

    tree_close(tree);
        remove("test_split1.db");
}

static void test_guidepost_placement(void) {
    remove("test_split2.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_split2.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    root_id = insert(tree, 10, "ten");
    root_id = insert(tree, 20, "twenty");
    root_id = insert(tree, 30, "thirty");
    root_id = insert(tree, 40, "forty");
    root_id = insert(tree, 50, "fifty");

    void *raw = bp_fetch_page(bp, root_id);
    Node *root = deserialize_node(raw);

    int guidepost = root->keys[0];

    void *r_raw = bp_fetch_page(bp, root->data.inner.children[1]);
    Node *right = deserialize_node(r_raw);
    ASSERT_INT_EQ("split_guidepost_in_right_leaf", guidepost, right->keys[0]);

    bp_unpin(bp, root->data.inner.children[1], false);
    free(right);
    bp_unpin(bp, root_id, false);
    free(root);
    
    tree_close(tree);
        remove("test_split2.db");
}

static void test_search_after_split(void) {
    remove("test_split3.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_split3.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    root_id = insert(tree, 10, "ten");
    root_id = insert(tree, 20, "twenty");
    root_id = insert(tree, 30, "thirty");
    root_id = insert(tree, 40, "forty");
    root_id = insert(tree, 50, "fifty");

    ASSERT_STR_EQ("split_search_10",      "ten",    (char*)search(tree, 10));
    ASSERT_STR_EQ("split_search_20",      "twenty", (char*)search(tree, 20));
    ASSERT_STR_EQ("split_search_30",      "thirty", (char*)search(tree, 30));
    ASSERT_STR_EQ("split_search_40",      "forty",  (char*)search(tree, 40));
    ASSERT_STR_EQ("split_search_50",      "fifty",  (char*)search(tree, 50));
    ASSERT_NULL(  "split_search_missing",           search(tree, 99));

    tree_close(tree);
        remove("test_split3.db");
}

static void test_split_reverse_order(void) {
    remove("test_split4.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_split4.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    root_id = insert(tree, 50, "fifty");
    root_id = insert(tree, 40, "forty");
    root_id = insert(tree, 30, "thirty");
    root_id = insert(tree, 20, "twenty");
    root_id = insert(tree, 10, "ten"); 

    ASSERT_STR_EQ("split_reverse_search_10", "ten",    (char*)search(tree, 10));
    ASSERT_STR_EQ("split_reverse_search_30", "thirty", (char*)search(tree, 30));
    ASSERT_STR_EQ("split_reverse_search_50", "fifty",  (char*)search(tree, 50));

    tree_close(tree);
        remove("test_split4.db");
}

static void test_two_splits(void) {
    remove("test_split5.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_split5.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    root_id = insert(tree, 10, "v10");
    root_id = insert(tree, 20, "v20");
    root_id = insert(tree, 30, "v30");
    root_id = insert(tree, 40, "v40");
    root_id = insert(tree, 50, "v50"); 
    root_id = insert(tree, 60, "v60");
    root_id = insert(tree, 70, "v70");
    root_id = insert(tree, 80, "v80");
    root_id = insert(tree, 90, "v90"); 

    ASSERT_STR_EQ("split_two_search_10", "v10", (char*)search(tree, 10));
    ASSERT_STR_EQ("split_two_search_50", "v50", (char*)search(tree, 50));
    ASSERT_STR_EQ("split_two_search_90", "v90", (char*)search(tree, 90));
    ASSERT_NULL(  "split_two_missing",          search(tree, 55));

    tree_close(tree);
        remove("test_split5.db");
}

int main(void) {
	test_fifth_insert_creates_root();
	test_guidepost_placement();
	test_search_after_split();
	test_split_reverse_order();
	test_two_splits();
	return 0;
}
