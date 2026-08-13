#include "src/bplustree.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define NUM_KEYS 1000

bool check_match(Tree *tree, bool *ref) {
    for (int i = 0; i < NUM_KEYS; i++) {
        void *val = search(tree, i);
        bool expected = ref[i];
        bool actual = (val != NULL);
        if (expected != actual) return false;
        if (expected && (int64_t)val != (i + 1)) return false;
    }
    return true;
}

int main() {
    FILE *tracker = fopen("stress_tracker.txt", "r");
    if (!tracker) {
        return 0; // If tracker wasn't created yet, nothing to verify
    }

    int k_tracker = -1;
    fscanf(tracker, "%d", &k_tracker);
    fclose(tracker);

    if (k_tracker < 0) {
        return 0;
    }

    Tree *tree = tree_open("test_stress.db", "test_stress.wal");

    // Replay deterministic sequence up to k_tracker
    srand(12345);
    bool ref[NUM_KEYS] = {false};

    int match_k = -2;

    // Check empty state (-1)
    if (k_tracker - 1 == -1 || k_tracker == -1) {
        if (check_match(tree, ref)) match_k = -1;
    }

    for (int k = 0; k <= k_tracker && match_k == -2; k++) {
        int op = rand() % 3;
        int key = rand() % NUM_KEYS;

        if (op == 0) {
            ref[key] = true;
        } else if (op == 1) {
            ref[key] = false;
        }

        if (k == k_tracker - 1 || k == k_tracker) {
            if (check_match(tree, ref)) {
                match_k = k;
                break;
            }
        }
    }

    tree_close(tree);

    if (match_k != -2) {
        printf("Recovery successful: B+Tree state exactly matches operation k=%d (tracker was %d)\n", match_k, k_tracker);
        return 0;
    } else {
        printf("Data integrity failure! Tree state does not match k=%d nor k=%d!\n", k_tracker - 1, k_tracker);
        return 1;
    }
}
