#include "tests/test_helpers.h"
#include "src/bplustree.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

int main(void) {
    remove("chaos_tracker.txt");
    remove("test_chaos.db");
    remove("test_chaos.wal");
    
    Tree *tree = tree_open("test_chaos.db", "test_chaos.wal");
    if (!tree) {
        return 1;
    }
    
    FILE *tracker = fopen("chaos_tracker.txt", "w");
    if (!tracker) return 1;
    
    for (int i = 0; i < 5000; i++) {
        // Insert key i with value i
        insert(tree, i, (void*)(int64_t)i);
        
        // Write the max key inserted so far (fsync tracker so verification knows what to expect)
        rewind(tracker);
        fprintf(tracker, "%d\n", i);
        fflush(tracker);
        fsync(fileno(tracker));
        
        // Sleep occasionally to give the chaos script a chance to kill this process
        if (i % 100 == 0) {
            usleep(1000); // 1 millisecond
        }
    }
    
    fclose(tracker);
    tree_close(tree);
    
    return 0;
}
