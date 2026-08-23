/*
 * crash_cycle.c   *** NEW FILE - added during the audit pass ***
 *
 * Crash target for tests/crash_cycle_test.py.
 *
 * The existing crash targets (tests/run_workload.c and tests/stress_workload.c)
 * both remove() the .db and .wal at startup, so every crash cycle begins with
 * an EMPTY write-ahead log.  That means tree_open() never takes the
 * wal_recover() + wal_clear() path with a pre-existing log, and a whole class
 * of recovery bugs is invisible to them.
 *
 * This target never wipes the database.  Each cycle resumes the same file, so
 * from the second cycle onward tree_open() must recover a non-empty WAL left
 * behind by the previous SIGKILL.
 *
 * Durability contract under test: a key is written to the tracker only AFTER
 * insert() returned (insert() fsyncs the WAL), so every key <= tracker must
 * survive a SIGKILL at any point.
 *
 * Modes:
 *   init    create a fresh empty database and tracker
 *   run     resume and insert keys forever (expects to be SIGKILLed)
 *   verify  reopen, replay recovery, and check every acked key survived
 */

#include "../src/bplustree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include "../src/compat.h"

#define DB_FILE  "test_crash_cycle.db"
#define WAL_FILE "test_crash_cycle.wal"
#define TRACKER     "crash_cycle_tracker.txt"
#define TRACKER_TMP "crash_cycle_tracker.tmp"

#define MAX_KEYS_TOTAL 4000

static int read_tracker(void)
{
    FILE *f = fopen(TRACKER, "r");
    if (!f) return -1;
    int k = -1;
    if (fscanf(f, "%d", &k) != 1) k = -1;
    fclose(f);
    return k;
}

/* Record k as durably acked.
 *
 * Written via write-temp + fsync + atomic rename, so a SIGKILL can never
 * leave a truncated or empty tracker behind.  A plain fopen(...,"w") would
 * truncate first, and a kill landing in that window would make the tracker
 * unreadable and silently turn the next verify into a no-op. */
static void write_tracker(int k)
{
    FILE *f = fopen(TRACKER_TMP, "w");
    if (!f) return;
    fprintf(f, "%d\n", k);
    fflush(f);
    fsync(fileno(f));
    fclose(f);

    atomic_replace(TRACKER_TMP, TRACKER);

    int dfd = open(".", O_RDONLY);
    if (dfd >= 0) { fsync(dfd); close(dfd); }   /* make the rename durable */
}

int main(int argc, char **argv)
{
    const char *mode = (argc > 1) ? argv[1] : "verify";

    if (strcmp(mode, "init") == 0) {
        remove(DB_FILE);
        remove(WAL_FILE);
        remove(TRACKER);
        remove(TRACKER_TMP);
        Tree *t = tree_open(DB_FILE, WAL_FILE);
        if (!t) { printf("init: tree_open failed\n"); return 1; }
        tree_close(t);
        write_tracker(-1);
        return 0;
    }

    if (strcmp(mode, "run") == 0) {
        Tree *t = tree_open(DB_FILE, WAL_FILE);
        if (!t) { printf("run: tree_open failed\n"); return 1; }

        int start = read_tracker() + 1;
        for (int k = start; k < MAX_KEYS_TOTAL; k++) {
            insert(t, k, (void *)(int64_t)(k + 1));
            write_tracker(k);          /* only after insert() fsynced the WAL */
            if (k % 20 == 0) usleep(700);   /* widen the SIGKILL window */
        }
        tree_close(t);
        return 0;
    }

    /* verify */
    int acked = read_tracker();
    Tree *t = tree_open(DB_FILE, WAL_FILE);
    if (!t) { printf("verify: tree_open failed (acked=%d)\n", acked); return 1; }

    int lost = 0, wrong = 0, first_lost = -1;
    for (int k = 0; k <= acked; k++) {
        void *v = search(t, k);
        if (v == NULL) {
            if (first_lost < 0) first_lost = k;
            lost++;
        } else if ((int64_t)v != (int64_t)(k + 1)) {
            wrong++;
        }
    }

    bool valid = validate_tree(t);
    tree_close(t);

    if (lost || wrong || !valid) {
        printf("verify FAILED: acked=%d lost=%d (first lost key=%d) wrong_value=%d "
               "tree_valid=%s\n",
               acked, lost, first_lost, wrong, valid ? "yes" : "NO");
        return 1;
    }

    printf("verify OK: acked=%d, all %d keys present, tree invariants hold\n",
           acked, acked + 1);
    return 0;
}
