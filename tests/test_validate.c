#include "test_helpers.h"
#include <time.h>
#include <limits.h>
#include <stdint.h>
#include "../src/buffer_pool.h"
#include "../src/page_manager.h"
#include "../src/serialize.h"

static void test_validate_empty_tree(void) {
    PageManager *pm = pm_open("test_val1.db");
    BufferPool *bp = bp_create(pm, "test_wal.log");
    
    ASSERT("validate_empty_tree", validate_tree(bp, INVALID_PAGE_ID), "INVALID_PAGE_ID root should be valid");
    
    bp_destroy(bp);
    pm_close(pm);
    remove("test_val1.db");
}

static void test_validate_single_leaf(void) {
    PageManager *pm = pm_open("test_val2.db");
    BufferPool *bp = bp_create(pm, "test_wal.log");
    page_id_t root = INVALID_PAGE_ID;
    
    root = insert(bp, root, 10, "ten");
    ASSERT("validate_single_leaf", validate_tree(bp, root), "single-key leaf should be valid");
    
    bp_destroy(bp);
    pm_close(pm);
    remove("test_val2.db");
}

static void test_validate_after_inserts(void) {
    PageManager *pm = pm_open("test_val3.db");
    BufferPool *bp = bp_create(pm, "test_wal.log");
    page_id_t root = INVALID_PAGE_ID;
    
    for (int i = 1; i <= 15; i++) {
        root = insert(bp, root, i * 10, (void*)(intptr_t)i);
        ASSERT("validate_after_each_insert", validate_tree(bp, root), "tree must be valid after every insert");
    }
    
    bp_destroy(bp);
    pm_close(pm);
    remove("test_val3.db");
}

static void test_validate_after_deletes(void) {
    PageManager *pm = pm_open("test_val4.db");
    BufferPool *bp = bp_create(pm, "test_wal.log");
    page_id_t root = INVALID_PAGE_ID;
    
    for (int i = 1; i <= 15; i++) {
        root = insert(bp, root, i * 10, (void*)(intptr_t)i);
    }

    int keys[] = {50, 80, 30, 120, 10, 70, 140};
    for (int k = 0; k < 7; k++) {
        root = deleteNode(bp, root, keys[k]);
        ASSERT("validate_after_each_delete", validate_tree(bp, root), "tree must be valid after every delete");
    }
    
    bp_destroy(bp);
    pm_close(pm);
    remove("test_val4.db");
}

#define REF_CAPACITY 20000

typedef struct {
    int  keys[REF_CAPACITY];
    int  size;
} Ref;

static void ref_insert(Ref *r, int key) {
    for (int i = 0; i < r->size; i++) {
        if (r->keys[i] == key) return;
    }
    r->keys[r->size++] = key;
}

static void ref_delete(Ref *r, int key) {
    for (int i = 0; i < r->size; i++) {
        if (r->keys[i] == key) {
            r->keys[i] = r->keys[--r->size];
            return;
        }
    }
}

static bool ref_contains(Ref *r, int key) {
    for (int i = 0; i < r->size; i++) {
        if (r->keys[i] == key) return true;
    }
    return false;
}

static int run_property_test(int n_ops, unsigned int seed) {
    PageManager *pm = pm_open("test_val_stress.db");
    BufferPool *bp = bp_create(pm, "test_wal.log");
    page_id_t root = INVALID_PAGE_ID;
    
    Ref  *ref  = calloc(1, sizeof(Ref));
    if (!ref) { fprintf(stderr, "OOM\n"); return 1; }

    srand(seed);

    for (int op = 0; op < n_ops; op++) {
        int key = (rand() % 500) + 1;

        if (rand() % 2 == 0) {
            if (!ref_contains(ref, key)) {
                root = insert(bp, root, key, (void*)(intptr_t)key);
            }
            ref_insert(ref, key);
        } else {
            if (ref_contains(ref, key)) {
                root = deleteNode(bp, root, key);
            }
            ref_delete(ref, key);
        }

        /* validate invariants only once every 100 ops to save time */
        if (op % 100 == 0) {
            if (!validate_tree(bp, root)) {
                fprintf(stderr, "validate_tree FAILED at op %d (key=%d)\n", op, key);
                free(ref);
                bp_destroy(bp);
                pm_close(pm);
                remove("test_val_stress.db");
                return 1;
            }
        }

        if (ref->size > 0) {
            int probe = ref->keys[rand() % ref->size];
            if (!search(bp, root, probe)) {
                fprintf(stderr, "search() FAILED at op %d: key %d in ref but not in tree\n", op, probe);
                free(ref);
                bp_destroy(bp);
                pm_close(pm);
                remove("test_val_stress.db");
                return 1;
            }
        }
    }

    free(ref);
    bp_destroy(bp);
    pm_close(pm);
    remove("test_val_stress.db");
    return 0;
}

static void test_stress_500(void) {
    int result = run_property_test(500, 42);
    ASSERT("stress_500_ops", result == 0, "tree corrupted during 500 random ops");
}

static void test_stress_2000(void) {
    int result = run_property_test(2000, 137);
    ASSERT("stress_2000_ops", result == 0, "tree corrupted during 2000 random ops");
}

int main(void) {
    test_validate_empty_tree();
    test_validate_single_leaf();
    test_validate_after_inserts();
    test_validate_after_deletes();
    
    test_stress_500();
    test_stress_2000();

    return 0;
}
