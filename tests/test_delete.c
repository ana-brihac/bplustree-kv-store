#include "test_helpers.h"
#include "../src/buffer_pool.h"
#include "../src/page_manager.h"
#include "../src/serialize.h"

static void test_delete_existing(void) {
    remove("test_delete1.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_delete1.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    
    root_id = insert(tree, 10, "ten");
    root_id = insert(tree, 20, "twenty");
    root_id = insert(tree, 30, "thirty");

    root_id = deleteNode(tree, 20);

    void *raw = bp_fetch_page(bp, root_id);
    Node *root = deserialize_node(raw);

    ASSERT_INT_EQ("delete_existing_decrements_num_keys", 2, root->num_keys);
    ASSERT_NULL("delete_existing_key_gone", search(tree, 20));
    ASSERT_STR_EQ("delete_existing_others_remain_10", "ten",    (char*)search(tree, 10));
    ASSERT_STR_EQ("delete_existing_others_remain_30", "thirty", (char*)search(tree, 30));
    ASSERT_INT_EQ("delete_shifts_keys_key0", 10, root->keys[0]);
    ASSERT_INT_EQ("delete_shifts_keys_key1", 30, root->keys[1]);

    bp_unpin(bp, root_id, false);
    free(root);
    tree_close(tree);
        remove("test_delete1.db");
}

static void test_delete_first_key(void) {
    remove("test_delete2.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_delete2.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    
    root_id = insert(tree, 10, "ten");
    root_id = insert(tree, 20, "twenty");
    root_id = insert(tree, 30, "thirty");

    root_id = deleteNode(tree, 10);

    void *raw = bp_fetch_page(bp, root_id);
    Node *root = deserialize_node(raw);

    ASSERT_INT_EQ("delete_first_decrements_num_keys", 2, root->num_keys);
    ASSERT_NULL("delete_first_key_gone", search(tree, 10));
    ASSERT_INT_EQ("delete_first_shifts_key0", 20, root->keys[0]);
    ASSERT_INT_EQ("delete_first_shifts_key1", 30, root->keys[1]);

    bp_unpin(bp, root_id, false);
    free(root);
    tree_close(tree);
        remove("test_delete2.db");
}

static void test_delete_missing(void) {
    remove("test_delete3.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_delete3.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    
    root_id = insert(tree, 10, "ten");
    root_id = insert(tree, 20, "twenty");
    root_id = insert(tree, 30, "thirty");

    root_id = deleteNode(tree, 999);

    void *raw = bp_fetch_page(bp, root_id);
    Node *root = deserialize_node(raw);

    ASSERT_INT_EQ("delete_missing_key_unchanged", 3, root->num_keys);

    bp_unpin(bp, root_id, false);
    free(root);
    tree_close(tree);
        remove("test_delete3.db");
}

static void test_delete_null(void) {
    remove("test_delete4.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_delete4.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    
    page_id_t result = deleteNode(tree, 10);
    ASSERT("delete_from_null", result == INVALID_PAGE_ID, "should return INVALID_PAGE_ID");
    
    tree_close(tree);
        remove("test_delete4.db");
}

static void test_delete_borrow_right(void) {
    remove("test_delete5.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_delete5.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    
    root_id = insert(tree, 10, "ten");
    root_id = insert(tree, 20, "twenty");
    root_id = insert(tree, 30, "thirty");
    root_id = insert(tree, 40, "forty");
    root_id = insert(tree, 50, "fifty");

    root_id = deleteNode(tree, 10);

    ASSERT_NULL("borrow_right_search_10_gone", (char*)search(tree, 10));
    ASSERT_STR_EQ("borrow_right_search_20", "twenty", (char*)search(tree, 20));
    ASSERT_STR_EQ("borrow_right_search_30", "thirty", (char*)search(tree, 30));
    ASSERT_STR_EQ("borrow_right_search_40", "forty", (char*)search(tree, 40));
    ASSERT_STR_EQ("borrow_right_search_50", "fifty", (char*)search(tree, 50));

    tree_close(tree);
        remove("test_delete5.db");
}

static void test_delete_borrow_left(void) {
    remove("test_delete6.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_delete6.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    
    root_id = insert(tree, 10, "ten");
    root_id = insert(tree, 20, "twenty");
    root_id = insert(tree, 30, "thirty");
    root_id = insert(tree, 40, "forty");
    root_id = insert(tree, 50, "fifty");

    root_id = deleteNode(tree, 50);

    ASSERT_NULL("borrow_left_search_50_gone", (char*)search(tree, 50));
    ASSERT_STR_EQ("borrow_left_search_10", "ten", (char*)search(tree, 10));
    ASSERT_STR_EQ("borrow_left_search_20", "twenty", (char*)search(tree, 20));
    ASSERT_STR_EQ("borrow_left_search_30", "thirty", (char*)search(tree, 30));
    ASSERT_STR_EQ("borrow_left_search_40", "forty", (char*)search(tree, 40));

    tree_close(tree);
        remove("test_delete6.db");
}

static void test_delete_merge(void) {
    remove("test_delete7.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_delete7.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    
    root_id = insert(tree, 10, "ten");
    root_id = insert(tree, 20, "twenty");
    root_id = insert(tree, 30, "thirty");
    root_id = insert(tree, 40, "forty");
    root_id = insert(tree, 50, "fifty");
    
    root_id = deleteNode(tree, 50); // borrow left
    root_id = deleteNode(tree, 40); // triggers merge!

    ASSERT_NULL("merge_search_40_gone", (char*)search(tree, 40));
    ASSERT_NULL("merge_search_50_gone", (char*)search(tree, 50));
    ASSERT_STR_EQ("merge_search_10", "ten", (char*)search(tree, 10));
    ASSERT_STR_EQ("merge_search_20", "twenty", (char*)search(tree, 20));
    ASSERT_STR_EQ("merge_search_30", "thirty", (char*)search(tree, 30));

    void *raw = bp_fetch_page(bp, root_id);
    Node *root = deserialize_node(raw);
    ASSERT("merge_root_is_leaf", root->is_leaf == true, "tree should have shrunk to a single leaf");

    bp_unpin(bp, root_id, false);
    free(root);
    tree_close(tree);
        remove("test_delete7.db");
}

static void test_delete_cascade_merge(void) {
    /*
     * Build a 2-level tree: root=[30], left=[10,20], right=[30,40,50]
     * Delete 30 from right (no underflow).  Delete 40 from right → right=[50]
     * underflows (1 < min=2), left=[10,20] has exactly 2 keys so borrow fails
     * (sibling->num_keys > MAX_KEYS/2 is 2 > 2 = false).  Merge is forced:
     * left absorbs right → [10,20,50].  Parent loses its only key → root
     * collapses.  Final state: single leaf root [10,20,50].
     */
    remove("test_delete_cascade.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_delete_cascade.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;

    root_id = insert(tree, 10, "ten");
    root_id = insert(tree, 20, "twenty");
    root_id = insert(tree, 30, "thirty");
    root_id = insert(tree, 40, "forty");
    root_id = insert(tree, 50, "fifty");

    root_id = deleteNode(tree, 30);
    root_id = deleteNode(tree, 40);

    void *raw = bp_fetch_page(bp, root_id);
    Node *root = deserialize_node(raw);
    ASSERT("cascade_root_is_leaf", root->is_leaf == true, "tree should have collapsed to single leaf");
    ASSERT_INT_EQ("cascade_root_num_keys", 3, root->num_keys);
    bp_unpin(bp, root_id, false);
    free(root);

    ASSERT_STR_EQ("cascade_search_10", "ten",   (char*)search(tree, 10));
    ASSERT_STR_EQ("cascade_search_20", "twenty", (char*)search(tree, 20));
    ASSERT_STR_EQ("cascade_search_50", "fifty",  (char*)search(tree, 50));
    ASSERT_NULL("cascade_search_30_gone", search(tree, 30));
    ASSERT_NULL("cascade_search_40_gone", search(tree, 40));

    tree_close(tree);
        remove("test_delete_cascade.db");
}

int main(void) {
    test_delete_existing();
    test_delete_first_key();
    test_delete_missing();
    test_delete_null();
    test_delete_borrow_right();
    test_delete_borrow_left();
    test_delete_merge();
    test_delete_cascade_merge();
    return 0;
}
