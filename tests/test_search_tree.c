#include "test_helpers.h"
#include "../src/buffer_pool.h"
#include "../src/page_manager.h"
#include "../src/serialize.h"

static void test_search_tree_existing(void) {
    PageManager *pm = pm_open("test_st1.db");
    BufferPool *bp = bp_create(pm);
    page_id_t root_id = INVALID_PAGE_ID;
    
    // Insert enough keys to cause a split (assuming MAX_KEYS = 4)
    root_id = insert(bp, root_id, 10, "ten");
    root_id = insert(bp, root_id, 20, "twenty");
    root_id = insert(bp, root_id, 30, "thirty");
    root_id = insert(bp, root_id, 40, "forty");
    root_id = insert(bp, root_id, 50, "fifty");

    ASSERT_STR_EQ("search_tree_left",  "ten",   (char*)search(bp, root_id, 10));
    ASSERT_STR_EQ("search_tree_mid",   "thirty",(char*)search(bp, root_id, 30));
    ASSERT_STR_EQ("search_tree_right", "fifty", (char*)search(bp, root_id, 50));

    bp_destroy(bp);
    pm_close(pm);
    remove("test_st1.db");
}

static void test_search_tree_missing(void) {
    PageManager *pm = pm_open("test_st2.db");
    BufferPool *bp = bp_create(pm);
    page_id_t root_id = INVALID_PAGE_ID;

    root_id = insert(bp, root_id, 10, "ten");
    root_id = insert(bp, root_id, 20, "twenty");
    root_id = insert(bp, root_id, 30, "thirty");
    root_id = insert(bp, root_id, 40, "forty");
    root_id = insert(bp, root_id, 50, "fifty");

    ASSERT_NULL("search_tree_missing_left",  search(bp, root_id, 5));
    ASSERT_NULL("search_tree_missing_mid",   search(bp, root_id, 25));
    ASSERT_NULL("search_tree_missing_right", search(bp, root_id, 99));

    bp_destroy(bp);
    pm_close(pm);
    remove("test_st2.db");
}

int main(void) {
    test_search_tree_existing();
    test_search_tree_missing();
    return 0;
}
