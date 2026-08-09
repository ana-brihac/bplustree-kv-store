#include "test_helpers.h"
#include <assert.h>
#include "../src/buffer_pool.h"
#include "../src/page_manager.h"
#include "../src/serialize.h"

void insert_keys(BufferPool *bp, page_id_t *root_id, int keys[], char *vals[], int n) {
    for (int i = 0; i < n; i++) {
        *root_id = insert(bp, *root_id, keys[i], vals[i]);
    }
}

void test_basic_range() {
    PageManager *pm = pm_open("test_rs1.db");
    BufferPool *bp = bp_create(pm);
    page_id_t root = INVALID_PAGE_ID;
    
    int keys[] = {10, 20, 30, 40, 50, 60, 70, 80};
    char *vals[] = {"v10", "v20", "v30", "v40", "v50", "v60", "v70", "v80"};
    insert_keys(bp, &root, keys, vals, 8);

    int res_keys[20];
    void *res_vals[20];
    int num_res = 0;

    range_search(bp, root, 30, 60, res_keys, res_vals, &num_res);
    
    ASSERT_INT_EQ("basic_range_count", 4, num_res);
    ASSERT_INT_EQ("basic_range_k0", 30, res_keys[0]);
    ASSERT_INT_EQ("basic_range_k1", 40, res_keys[1]);
    ASSERT_INT_EQ("basic_range_k2", 50, res_keys[2]);
    ASSERT_INT_EQ("basic_range_k3", 60, res_keys[3]);
    
    bp_destroy(bp);
    pm_close(pm);
    remove("test_rs1.db");
}

void test_range_outside() {
    PageManager *pm = pm_open("test_rs2.db");
    BufferPool *bp = bp_create(pm);
    page_id_t root = INVALID_PAGE_ID;
    
    int keys[] = {10, 20, 30, 40, 50};
    char *vals[] = {"v10", "v20", "v30", "v40", "v50"};
    insert_keys(bp, &root, keys, vals, 5);

    int res_keys[20];
    void *res_vals[20];
    int num_res = 0;

    range_search(bp, root, 5, 25, res_keys, res_vals, &num_res);
    
    ASSERT_INT_EQ("range_outside_count", 2, num_res);
    ASSERT_INT_EQ("range_outside_k0", 10, res_keys[0]);
    ASSERT_INT_EQ("range_outside_k1", 20, res_keys[1]);
    
    bp_destroy(bp);
    pm_close(pm);
    remove("test_rs2.db");
}

void test_range_all() {
    PageManager *pm = pm_open("test_rs3.db");
    BufferPool *bp = bp_create(pm);
    page_id_t root = INVALID_PAGE_ID;
    
    int keys[] = {10, 20, 30, 40, 50};
    char *vals[] = {"v10", "v20", "v30", "v40", "v50"};
    insert_keys(bp, &root, keys, vals, 5);

    int res_keys[20];
    void *res_vals[20];
    int num_res = 0;

    range_search(bp, root, 0, 100, res_keys, res_vals, &num_res);
    
    ASSERT_INT_EQ("range_all_count", 5, num_res);
    for (int i = 0; i < 5; i++) {
        ASSERT_INT_EQ("range_all_k_match", keys[i], res_keys[i]);
    }
    
    bp_destroy(bp);
    pm_close(pm);
    remove("test_rs3.db");
}

void test_range_empty() {
    PageManager *pm = pm_open("test_rs4.db");
    BufferPool *bp = bp_create(pm);
    page_id_t root = INVALID_PAGE_ID;
    
    int res_keys[20];
    void *res_vals[20];
    int num_res = 0;

    bool found = range_search(bp, root, 10, 20, res_keys, res_vals, &num_res);
    ASSERT("range_empty_not_found", found == false, "should be false");
    ASSERT_INT_EQ("range_empty_count", 0, num_res);
    
    bp_destroy(bp);
    pm_close(pm);
    remove("test_rs4.db");
}

void test_range_cross_leaf_boundary(void) {
    /*
     * 6 insertions with MAX_KEYS=4 forces a leaf split:
     *   root = [30], left = [10,20], right = [30,40,50,60]
     * A range [20,40] straddles both leaves and must follow the next_id link.
     */
    PageManager *pm = pm_open("test_rs5.db");
    BufferPool *bp = bp_create(pm);
    page_id_t root = INVALID_PAGE_ID;

    int keys[] = {10, 20, 30, 40, 50, 60};
    char *vals[] = {"v10", "v20", "v30", "v40", "v50", "v60"};
    insert_keys(bp, &root, keys, vals, 6);

    int res_keys[20];
    void *res_vals[20];
    int num_res = 0;

    range_search(bp, root, 20, 40, res_keys, res_vals, &num_res);

    /* Expect keys 20 (left leaf) and 30, 40 (right leaf) */
    ASSERT_INT_EQ("cross_boundary_count", 3, num_res);
    ASSERT_INT_EQ("cross_boundary_k0", 20, res_keys[0]);
    ASSERT_INT_EQ("cross_boundary_k1", 30, res_keys[1]);
    ASSERT_INT_EQ("cross_boundary_k2", 40, res_keys[2]);

    bp_destroy(bp);
    pm_close(pm);
    remove("test_rs5.db");
}

int main(void) {
    test_basic_range();
    test_range_outside();
    test_range_all();
    test_range_empty();
    test_range_cross_leaf_boundary();
    return 0;
}
