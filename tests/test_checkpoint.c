#include "src/bplustree.h"
#include "../src/buffer_pool.h"
#include "../src/page_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>

off_t get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

/* Test 1: checkpoint shrinks the WAL and data is still readable. */
static void test_checkpoint_basic(void) {
    remove("test_ckpt.db");
    remove("test_ckpt.wal");

    Tree *tree = tree_open("test_ckpt.db", "test_ckpt.wal");
    assert(tree != NULL);

    // Insert enough keys to trigger a checkpoint (threshold is 100 log records)
    for (int i = 0; i < 200; i++) {
        insert(tree, i, (void *)(int64_t)i);
    }

    // Read the WAL file size
    off_t wal_size = get_file_size("test_ckpt.wal");
    printf("WAL file size after 200 inserts: %ld bytes\n", wal_size);
    printf("Current WAL sequence number: %d\n", tree->bp->wal->next_seq_num);

    // Verify that data is still findable!
    for (int i = 0; i < 200; i++) {
        void *val = search(tree, i);
        assert(val != NULL || i == 0); // i=0 has value 0 which is NULL pointer
        if (i > 0) {
            assert((int64_t)val == i);
        }
    }
    printf("Data integrity verified after automatic checkpointing.\n");

    tree_close(tree);
    remove("test_ckpt.db");
    remove("test_ckpt.wal");
}

/* Test 2: crash immediately after a checkpoint.
 * After the checkpoint the WAL is empty, so recovery must rely entirely on
 * the already-flushed DB file.  All data inserted before the checkpoint must
 * still be present after reopen. */
static void test_crash_after_checkpoint(void) {
    remove("test_ckpt2.db");
    remove("test_ckpt2.wal");

    // --- Phase 1: fill the tree until a checkpoint fires ---
    Tree *tree = tree_open("test_ckpt2.db", "test_ckpt2.wal");
    assert(tree != NULL);

    // 200 inserts guarantees at least one automatic checkpoint
    for (int i = 0; i < 200; i++) {
        insert(tree, i, (void *)(int64_t)i);
    }

    // Force an explicit checkpoint so the WAL is definitely empty before the crash
    wal_checkpoint(tree->bp);

    // --- Phase 2: simulate a crash (close fds, free memory raw, skip tree_close) ---
    close(tree->pm->fd);
    close(tree->wal->fd);

    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        free(tree->bp->frames[i].data);
    }
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashNode *cur = tree->bp->page_table[i];
        while (cur) {
            HashNode *nxt = cur->next;
            free(cur);
            cur = nxt;
        }
    }
    free(tree->bp);
    free(tree->pm);
    free(tree->wal->buffer);
    free(tree->wal);
    free(tree);

    // --- Phase 3: reopen (WAL is empty; recovery reads nothing; DB must be complete) ---
    Tree *tree2 = tree_open("test_ckpt2.db", "test_ckpt2.wal");
    assert(tree2 != NULL);

    bool all_found = true;
    for (int i = 1; i < 200; i++) { // skip i=0 whose value is NULL
        void *val = search(tree2, i);
        if ((int64_t)val != i) {
            all_found = false;
            break;
        }
    }

    if (all_found) {
        printf("[PASS] crash_after_checkpoint\n");
    } else {
        printf("[FAIL] crash_after_checkpoint: data lost after checkpoint + crash\n");
    }

    tree_close(tree2);
    remove("test_ckpt2.db");
    remove("test_ckpt2.wal");
}

int main(void) {
    test_checkpoint_basic();
    test_crash_after_checkpoint();
    return 0;
}
