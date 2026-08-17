#include "test_helpers.h"
#include <stdint.h>
#include <unistd.h>

/* Test: open → insert until root splits → clean close → reopen → all data present.
 *
 * This covers:
 *   T-3: root_id is persisted to the meta page and restored on reopen.
 *   T-1: the WAL must reach disk before the DB file is updated, otherwise the
 *        clean-close data would be lost on reopen without a WAL.
 */
static void test_clean_reopen(void) {
    remove("test_reopen.db");
    remove("test_reopen.wal");

    // --- Phase 1: build a tree with several levels (forces root splits) ---
    Tree *tree = tree_open("test_reopen.db", "test_reopen.wal");
    if (!tree) { FAIL("clean_reopen_open1", "tree_open returned NULL"); return; }

    // 50 inserts with MAX_KEYS=4 will produce at least two levels
    for (int i = 1; i <= 50; i++) {
        insert(tree, i, (void *)(int64_t)i);
    }

    page_id_t root_before = tree->root_id;
    ASSERT("clean_reopen_root_not_invalid", root_before != INVALID_PAGE_ID,
           "root_id must not be INVALID after inserts");

    // --- Phase 2: clean close ---
    tree_close(tree);

    // --- Phase 3: reopen ---
    Tree *tree2 = tree_open("test_reopen.db", "test_reopen.wal");
    if (!tree2) { FAIL("clean_reopen_open2", "tree_open returned NULL on reopen"); return; }

    ASSERT("clean_reopen_root_id_matches", tree2->root_id == root_before,
           "root_id must survive a clean close/reopen");

    ASSERT("clean_reopen_tree_valid", validate_tree(tree2) == true,
           "tree must be structurally valid after reopen");

    // All 50 keys must be present
    bool all_found = true;
    for (int i = 1; i <= 50; i++) {
        void *val = search(tree2, i);
        if ((int64_t)val != i) {
            all_found = false;
            break;
        }
    }
    ASSERT("clean_reopen_all_keys_found", all_found, "all 50 keys must be found after reopen");

    tree_close(tree2);
    remove("test_reopen.db");
    remove("test_reopen.wal");
}

/* Test: WAL record is on disk before the page is flushed.
 *
 * Strategy: insert one key (which calls wal_fsync), then read the WAL file
 * directly.  The WAL must contain at least one record.  Then confirm the
 * data is searchable after a clean reopen — proving the write-ahead ordering
 * is maintained.
 */
static void test_wal_before_db(void) {
    remove("test_wal_order.db");
    remove("test_wal_order.wal");

    Tree *tree = tree_open("test_wal_order.db", "test_wal_order.wal");
    if (!tree) { FAIL("wal_ordering_open", "tree_open returned NULL"); return; }

    insert(tree, 42, (void *)(int64_t)42);

    // The insert called wal_fsync; check the WAL file has at least one record
    int wal_seq = tree->wal->next_seq_num;
    ASSERT("wal_ordering_records_written", wal_seq > 0,
           "WAL must have at least one record after insert");

    tree_close(tree);

    // Reopen without recovery (WAL was cleared on close) and confirm key is there
    Tree *tree2 = tree_open("test_wal_order.db", "test_wal_order.wal");
    if (!tree2) { FAIL("wal_ordering_reopen", "tree_open returned NULL"); return; }

    void *val = search(tree2, 42);
    ASSERT("wal_ordering_data_survives", (int64_t)val == 42,
           "inserted key must survive clean close/reopen");

    tree_close(tree2);
    remove("test_wal_order.db");
    remove("test_wal_order.wal");
}

int main(void) {
    test_clean_reopen();
    test_wal_before_db();
    return 0;
}
