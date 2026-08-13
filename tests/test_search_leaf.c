#include "test_helpers.h"
#include "../src/buffer_pool.h"
#include "../src/page_manager.h"
#include "../src/serialize.h"

static void test_search_leaf_existing(void) {
    PageManager *pm = pm_open("test_sl1.db");
    BufferPool *bp = bp_create(pm, "test_wal.log");
    page_id_t root_id = INVALID_PAGE_ID;
    
    root_id = insert(bp, root_id, 10, "ten");
    root_id = insert(bp, root_id, 20, "twenty");
    root_id = insert(bp, root_id, 30, "thirty");

    ASSERT_STR_EQ("search_leaf_mid",   "twenty", (char*)search(bp, root_id, 20));
    ASSERT_STR_EQ("search_leaf_first", "ten",    (char*)search(bp, root_id, 10));
    ASSERT_STR_EQ("search_leaf_last",  "thirty", (char*)search(bp, root_id, 30));

    bp_destroy(bp);
    pm_close(pm);
    remove("test_sl1.db");
}

static void test_search_leaf_missing(void) {
    PageManager *pm = pm_open("test_sl2.db");
    BufferPool *bp = bp_create(pm, "test_wal.log");
    page_id_t root_id = INVALID_PAGE_ID;

    root_id = insert(bp, root_id, 10, "ten");
    root_id = insert(bp, root_id, 20, "twenty");
    root_id = insert(bp, root_id, 30, "thirty");

    ASSERT_NULL("search_leaf_missing_between", search(bp, root_id, 15));
    ASSERT_NULL("search_leaf_missing_before",  search(bp, root_id, 5));
    ASSERT_NULL("search_leaf_missing_after",   search(bp, root_id, 99));

    bp_destroy(bp);
    pm_close(pm);
    remove("test_sl2.db");
}

static void test_search_leaf_empty(void) {
    PageManager *pm = pm_open("test_sl3.db");
    BufferPool *bp = bp_create(pm, "test_wal.log");
    page_id_t root_id = INVALID_PAGE_ID;

    ASSERT_NULL("search_leaf_empty_tree", search(bp, root_id, 10));

    bp_destroy(bp);
    pm_close(pm);
    remove("test_sl3.db");
}

int main(void) {
    test_search_leaf_existing();
    test_search_leaf_missing();
    test_search_leaf_empty();
    return 0;
}
