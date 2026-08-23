#include "test_helpers.h"
#include "../src/buffer_pool.h"
#include "../src/page_manager.h"
#include "../src/serialize.h"

static void test_search_tree_existing(void) {
    remove("test_st1.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_st1.db", "test_wal.log");
    
    // Insert enough keys to cause a split (assuming MAX_KEYS = 4)
    insert(tree, 10, "ten");
    insert(tree, 20, "twenty");
    insert(tree, 30, "thirty");
    insert(tree, 40, "forty");
    insert(tree, 50, "fifty");

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

    insert(tree, 10, "ten");
    insert(tree, 20, "twenty");
    insert(tree, 30, "thirty");
    insert(tree, 40, "forty");
    insert(tree, 50, "fifty");

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
