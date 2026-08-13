#include "tests/test_helpers.h"
#include "src/bplustree.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    Tree *tree = tree_open("test_chaos.db", "test_chaos.wal");
    if (!tree) {
        printf("Failed to open tree\n");
        // Removed 
        return 1;
    }
    
    FILE *tracker = fopen("chaos_tracker.txt", "r");
    if (!tracker) {
        tree_close(tree);
        return 0;
    }
    
    int max_key = -1;
    if (fscanf(tracker, "%d", &max_key) != 1) {
        max_key = -1;
    }
    fclose(tracker);
    
    if (max_key >= 0) {
        if (!validate_tree(tree)) {
            printf("Tree validation failed after recovery up to key %d!\n", max_key);
            tree_close(tree);
            return 1;
        }
        
        for (int i = 0; i <= max_key; i++) {
            void *val = search(tree, i);
            if ((int64_t)val != i) {
                printf("Data integrity failure: key %d not found! Expected %d, Got %ld\n", i, i, (int64_t)val);
                tree_close(tree);
                return 1;
            }
        }
        printf("Recovery successful: all %d keys verified!\n", max_key + 1);
    }
    
    tree_close(tree);
    return 0;
}
