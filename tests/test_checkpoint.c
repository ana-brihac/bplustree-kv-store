#include "src/bplustree.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdint.h>

off_t get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

int main() {
    remove("test_ckpt.db");
    remove("test_ckpt.wal");

    Tree *tree = tree_open("test_ckpt.db", "test_ckpt.wal");
    
    // Insert enough keys to trigger a checkpoint (threshold is 100 log records)
    // 50 inserts will likely cause ~150-200 log records because of splits
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
    
    return 0;
}
