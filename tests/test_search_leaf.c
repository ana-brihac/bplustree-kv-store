#include "test_helpers.h"
#include "../src/buffer_pool.h"
#include "../src/page_manager.h"
#include "../src/serialize.h"

static void test_search_leaf_existing(void) {
    remove("test_sl1.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_sl1.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    
    root_id = insert(tree, 10, "ten");
    root_id = insert(tree, 20, "twenty");
    root_id = insert(tree, 30, "thirty");

    ASSERT_STR_EQ("search_leaf_mid",   "twenty", (char*)search(tree, 20));
    ASSERT_STR_EQ("search_leaf_first", "ten",    (char*)search(tree, 10));
    ASSERT_STR_EQ("search_leaf_last",  "thirty", (char*)search(tree, 30));

    tree_close(tree);
        remove("test_sl1.db");
}

static void test_search_leaf_missing(void) {
    remove("test_sl2.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_sl2.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;

    root_id = insert(tree, 10, "ten");
    root_id = insert(tree, 20, "twenty");
    root_id = insert(tree, 30, "thirty");

    ASSERT_NULL("search_leaf_missing_between", search(tree, 15));
    ASSERT_NULL("search_leaf_missing_before",  search(tree, 5));
    ASSERT_NULL("search_leaf_missing_after",   search(tree, 99));

    tree_close(tree);
        remove("test_sl2.db");
}

static void test_search_leaf_empty(void) {
    remove("test_sl3.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_sl3.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;

    ASSERT_NULL("search_leaf_empty_tree", search(tree, 10));

    tree_close(tree);
        remove("test_sl3.db");
}

int main(void) {
    test_search_leaf_existing();
    test_search_leaf_missing();
    test_search_leaf_empty();
    return 0;
}
