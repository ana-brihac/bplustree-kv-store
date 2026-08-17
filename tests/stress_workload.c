#include "src/bplustree.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#define NUM_KEYS 1000

int main() {
    remove("stress_tracker.txt");
    remove("test_stress.db");
    remove("test_stress.wal");

    Tree *tree = tree_open("test_stress.db", "test_stress.wal");
    
    FILE *tracker = fopen("stress_tracker.txt", "w");
    if (!tracker) return 1;

    srand(12345); // Deterministic seed

    for (int k = 0; ; k++) {
        // Write the current operation index before executing it
        fseek(tracker, 0, SEEK_SET);
        fprintf(tracker, "%d\n", k);
        fflush(tracker);
        fsync(fileno(tracker));

        // Generate deterministic operation
        int op = rand() % 3; // 0: Insert, 1: Delete, 2: Search
        int key = rand() % NUM_KEYS;

        if (op == 0) {
            insert(tree, key, (void *)(int64_t)(key + 1));
        } else if (op == 1) {
            deleteNode(tree, key);
        } else {
            search(tree, key);
        }
        
        // Artificial small delay to increase chance of being killed mid-operation
        if (k % 50 == 0) {
            usleep(1000); 
        }
    }

    tree_close(tree);
    return 0;
}
