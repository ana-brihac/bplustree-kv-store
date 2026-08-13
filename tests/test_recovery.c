#include "test_helpers.h"
#include "../src/buffer_pool.h"
#include "../src/page_manager.h"
#include "../src/serialize.h"
#include <unistd.h>

static void test_recovery_basic(void) {
    remove("test_rec1.db");
    remove("test_rec1.wal");
    
    // 1. Open the tree and insert data
    Tree *tree = tree_open("test_rec1.db", "test_rec1.wal");
    
    // Insert 100 keys to cause multiple splits
    for (int i = 0; i < 100; i++) {
        insert(tree, i, (void*)(int64_t)i);
    }
    
    // Validate tree before crash
    ASSERT("tree_valid_before_crash", validate_tree(tree) == true, "Tree must be valid before crash");
    
    // 2. Simulate Crash!
    // We do NOT call tree_close(tree).
    // This orphans the buffer pool, leaving dirty pages in memory and NOT in test_rec1.db.
    // We only close the file descriptors so they can be reopened.
    close(tree->pm->fd);
    close(tree->wal->fd);
    free(tree->bp);
    free(tree->pm);
    free(tree->wal);
    free(tree);
    
    // At this point, test_rec1.db is MISSING the dirty pages from memory.
    // But test_rec1.wal has all the redo logs!
    
    // 3. Reopen the database (triggers recovery)
    Tree *tree2 = tree_open("test_rec1.db", "test_rec1.wal");
    
    ASSERT("tree2_valid_after_recovery", validate_tree(tree2) == true, "Tree must be valid after recovery");
    
    // Verify all keys are present
    bool all_found = true;
    for (int i = 0; i < 100; i++) {
        void *val = search(tree2, i);
        if ((int64_t)val != i) {
            all_found = false;
            break;
        }
    }
    ASSERT("all_keys_found_after_recovery", all_found == true, "All 100 keys must be found after recovery");
    
    tree_close(tree2);
    
    remove("test_rec1.db");
    remove("test_rec1.wal");
}

int main(void) {
    test_recovery_basic();
    return 0;
}
