#include "test_helpers.h"
#include "../src/buffer_pool.h"
#include "../src/page_manager.h"
#include "../src/serialize.h"

static void test_search_tree_existing(void) {
    remove("test_st1.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_st1.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    
    // Insert enough keys to cause a split (assuming MAX_KEYS = 4)
    root_id = insert(tree, 10, "ten");
    root_id = insert(tree, 20, "twenty");
    root_id = insert(tree, 30, "thirty");
    root_id = insert(tree, 40, "forty");
    root_id = insert(tree, 50, "fifty");

    ASSERT_STR_EQ("search_tree_left",  "ten",   (char*)search(tree, 10));
    ASSERT_STR_EQ("search_tree_mid",   "thirty",(char*)search(tree, 30));
    ASSERT_STR_EQ("search_tree_right", "fifty", (char*)search(tree, 50));

    tree_close(tree);
        remove("test_st1.db");
}

static void test_search_tree_missing(void) {
    remove("test_st2.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_st2.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;

    root_id = insert(tree, 10, "ten");
    root_id = insert(tree, 20, "twenty");
    root_id = insert(tree, 30, "thirty");
    root_id = insert(tree, 40, "forty");
    root_id = insert(tree, 50, "fifty");

    ASSERT_NULL("search_tree_missing_left",  search(tree, 5));
    ASSERT_NULL("search_tree_missing_mid",   search(tree, 25));
    ASSERT_NULL("search_tree_missing_right", search(tree, 99));

    tree_close(tree);
        remove("test_st2.db");
}

int main(void) {
    test_search_tree_existing();
    test_search_tree_missing();
    return 0;
}
