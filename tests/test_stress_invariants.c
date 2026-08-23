/*
 * test_stress_invariants.c   *** NEW FILE - added during the audit pass ***
 *
 * Randomised insert/delete/search stress test.
 *
 * Differs from the existing tests/stress_workload.c (which is a crash target
 * for final_stress_test.py and never validates structure): this one runs
 * in-process and, after EVERY single operation, checks
 *
 *   1. validate_tree()  - all B+Tree invariants: fanout bounds, min-key
 *                         occupancy, per-node key ordering, uniform leaf
 *                         depth, leaf sibling-chain ordering and the
 *                         key/child separation invariant.
 *   2. the full key space against a plain array reference model.
 *
 * Phase 2 additionally reopens the tree to check that a clean close +
 * reopen preserves the exact same contents.
 *
 * Values are always key+1 so the test does not depend on whether inserting
 * an already-present key overwrites or ignores.
 *
 * Usage: test_stress_invariants [ops] [keyspace] [seed]
 */

#include "test_helpers.h"

#include <stdint.h>
#include <stdbool.h>

#define DB_FILE  "test_stress_invariants.db"
#define WAL_FILE "test_stress_invariants.wal"

static int  KEYSPACE = 200;
static bool ref[4096];               /* ref[k] = key k is present */

static int failures = 0;

static void report(const char *what, int op_index, int key)
{
    if (failures < 10)
        printf("[FAIL] %s (op %d, key %d)\n", what, op_index, key);
    failures++;
}

/* Compare the whole key space against the reference model. */
static bool full_sweep(Tree *tree, int op_index)
{
    for (int k = 0; k < KEYSPACE; k++) {
        void *v = search(tree, k);
        if (ref[k]) {
            if (v == NULL) { report("missing key", op_index, k); return false; }
            if ((int64_t)v != (int64_t)(k + 1)) {
                report("wrong value", op_index, k);
                return false;
            }
        } else if (v != NULL) {
            report("phantom key", op_index, k);
            return false;
        }
    }
    return true;
}

/* Cross-check range_search over the whole key space against the model. */
static bool check_range(Tree *tree, int op_index)
{
    int    keys[4096];
    void  *vals[4096];
    int    n = 0;

    if (!range_search(tree, 0, KEYSPACE - 1, keys, vals, &n)) {
        /* range_search returns false only for an empty tree */
        for (int k = 0; k < KEYSPACE; k++)
            if (ref[k]) { report("range_search said empty", op_index, k); return false; }
        return true;
    }

    int expected = 0;
    for (int k = 0; k < KEYSPACE; k++) expected += ref[k] ? 1 : 0;

    if (n != expected) {
        printf("[FAIL] range_search count %d, expected %d (op %d)\n",
               n, expected, op_index);
        failures++;
        return false;
    }
    for (int i = 0; i < n; i++) {
        if (!ref[keys[i]])            { report("range_search phantom", op_index, keys[i]); return false; }
        if (i > 0 && keys[i] <= keys[i - 1]) {
            report("range_search out of order", op_index, keys[i]);
            return false;
        }
        if ((int64_t)vals[i] != (int64_t)(keys[i] + 1)) {
            report("range_search wrong value", op_index, keys[i]);
            return false;
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    int ops  = (argc > 1) ? atoi(argv[1]) : 10000;
    KEYSPACE = (argc > 2) ? atoi(argv[2]) : 200;
    unsigned seed = (argc > 3) ? (unsigned)atoi(argv[3]) : 20260823u;

    if (KEYSPACE > 4096) KEYSPACE = 4096;

    remove(DB_FILE);
    remove(WAL_FILE);

    Tree *tree = tree_open(DB_FILE, WAL_FILE);
    if (!tree) { printf("[FAIL] stress_tree_open\n"); return 1; }

    srand(seed);
    for (int k = 0; k < KEYSPACE; k++) ref[k] = false;

    int n_ins = 0, n_del = 0, n_srch = 0;

    /* ---------- phase 1: random ops, validated after each one ---------- */
    for (int i = 0; i < ops && failures == 0; i++) {
        int op  = rand() % 3;
        int key = rand() % KEYSPACE;

        if (op == 0) {
            insert(tree, key, (void *)(int64_t)(key + 1));
            ref[key] = true;
            n_ins++;
        } else if (op == 1) {
            deleteNode(tree, key);
            ref[key] = false;
            n_del++;
        } else {
            void *v = search(tree, key);
            n_srch++;
            if (ref[key] && (v == NULL || (int64_t)v != (int64_t)(key + 1)))
                report("search miss", i, key);
            if (!ref[key] && v != NULL)
                report("search phantom", i, key);
        }

        if (!validate_tree(tree)) {
            printf("[FAIL] tree invariant violated after op %d "
                   "(op=%d key=%d)\n", i, op, key);
            failures++;
            break;
        }
        if (!full_sweep(tree, i)) break;

        /* range_search is O(keyspace); every 250 ops is enough coverage */
        if (i % 250 == 0 && !check_range(tree, i)) break;
    }

    if (failures == 0) {
        printf("[PASS] stress_invariants_%d_ops\n", ops);
        printf("       inserts=%d deletes=%d searches=%d keyspace=%d seed=%u\n",
               n_ins, n_del, n_srch, KEYSPACE, seed);
    }

    /* ---------- phase 2: clean close + reopen must preserve contents ---------- */
    tree_close(tree);

    tree = tree_open(DB_FILE, WAL_FILE);
    if (!tree) {
        printf("[FAIL] stress_reopen_tree_open\n");
        return 1;
    }
    int reopen_failures = failures;
    if (full_sweep(tree, -1) && validate_tree(tree))
        printf("[PASS] stress_reopen_preserves_contents\n");
    else if (failures > reopen_failures)
        printf("[FAIL] stress_reopen_preserves_contents\n");
    tree_close(tree);

    remove(DB_FILE);
    remove(WAL_FILE);

    if (failures) {
        printf("\n%d failure(s)\n", failures);
        return 1;
    }
    return 0;
}
