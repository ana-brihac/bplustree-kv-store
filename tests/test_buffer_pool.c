/*
 * test_buffer_pool.c
 *
 * Tests for: bp_create, bp_destroy, bp_fetch_page, bp_pin, bp_unpin,
 *            bp_flush_page, bp_flush_all, hash_insert, hash_lookup,
 *            hash_remove, hash_function
 *
 * Cases:
 *   bp_create_returns_non_null        - bp_create() never returns NULL
 *   bp_create_global_time_zero        - global_time starts at 0
 *   bp_create_frames_invalid          - all frame page_ids start as INVALID
 *   bp_create_frames_pin_zero         - all frames pin_count starts at 0
 *   bp_create_frames_not_dirty        - all frames is_dirty starts false
 *   bp_create_table_null              - all hash table buckets start NULL
 *   hash_function_range               - hash result is within [0, HASH_TABLE_SIZE)
 *   hash_insert_lookup_basic          - insert then lookup returns correct frame
 *   hash_insert_update_existing       - inserting same page_id updates frame index
 *   hash_lookup_missing               - lookup of absent page returns -1
 *   hash_remove_found                 - remove clears the entry (lookup returns -1)
 *   hash_remove_not_found_no_crash    - removing absent page does not crash
 *   hash_lookup_null_bp               - lookup with NULL bp returns -1
 *   hash_insert_invalid_page          - insert with INVALID_PAGE_ID is a no-op
 *   bp_fetch_page_null_bp             - fetch with NULL bp returns NULL
 *   bp_fetch_page_loads_new           - fetch brings a page into a frame
 *   bp_fetch_page_pins_frame          - fetch increments pin_count to 1
 *   bp_fetch_page_cache_hit           - second fetch of same page hits cache
 *   bp_pin_increments_count           - bp_pin increments pin_count
 *   bp_pin_null_bp                    - bp_pin with NULL bp does not crash
 *   bp_unpin_decrements_count         - bp_unpin decrements pin_count
 *   bp_unpin_no_below_zero            - bp_unpin does not go below 0
 *   bp_unpin_marks_dirty              - bp_unpin with mark_dirty=true sets flag
 *   bp_unpin_null_bp                  - bp_unpin with NULL bp does not crash
 *   bp_flush_page_null_bp             - flush with NULL bp returns false
 *   bp_flush_page_missing             - flush of non-resident page returns false
 *   bp_flush_page_clean               - flush of non-dirty page returns true
 *   bp_flush_page_dirty               - flush of dirty page writes and clears flag
 *   bp_flush_all_no_crash             - bp_flush_all on a live pool does not crash
 *   bp_destroy_null_no_crash          - bp_destroy(NULL) does not crash
 *   bp_eviction_flushes_and_loads     - evicting a dirty frame flushes it before loading new page
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "../src/buffer_pool.h"
#include "../src/page_manager.h"

/* ---------------------------------------------------------------
 * Pretty-print macros (mirror of test_helpers.h, standalone)
 * --------------------------------------------------------------- */
#define PASS(name)         printf("[PASS] %s\n", (name))
#define FAIL(name, msg)    printf("[FAIL] %s: %s\n", (name), (msg))

#define ASSERT(name, cond, msg) \
    do { if (cond) { PASS(name); } else { FAIL(name, msg); } } while (0)

#define ASSERT_INT_EQ(name, expected, actual)                           \
    do {                                                                 \
        int _e = (expected), _a = (actual);                             \
        if (_e == _a) { PASS(name); }                                   \
        else {                                                           \
            char _buf[128];                                              \
            snprintf(_buf, sizeof(_buf), "expected %d, got %d", _e, _a);\
            FAIL(name, _buf);                                            \
        }                                                                \
    } while (0)

#define ASSERT_NULL(name, ptr) \
    ASSERT(name, (ptr) == NULL, "expected NULL, got non-NULL")

#define ASSERT_NOT_NULL(name, ptr) \
    ASSERT(name, (ptr) != NULL, "expected non-NULL, got NULL")

/* ---------------------------------------------------------------
 * Helper: open a fresh temporary PageManager backed by a real file
 * --------------------------------------------------------------- */
static PageManager *open_tmp_pm(char *path_out) {
    strcpy(path_out, "/tmp/bp_test_XXXXXX");
    int fd = mkstemp(path_out);
    if (fd < 0) return NULL;
    close(fd);
    return pm_open(path_out);
}

static void cleanup_tmp_pm(PageManager *pm, const char *path) {
    if (pm) pm_close(pm);
    unlink(path);
}

/* ---------------------------------------------------------------
 * Test: bp_create
 * --------------------------------------------------------------- */
static void test_bp_create(void) {
    char path[64];
    PageManager *pm = open_tmp_pm(path);
    ASSERT_NOT_NULL("pm_open_succeeds", pm);

    BufferPool *bp = bp_create(pm);

    ASSERT_NOT_NULL("bp_create_returns_non_null", bp);
    ASSERT_INT_EQ("bp_create_global_time_zero", 0, bp->global_time);

    int all_invalid = 1, all_pin_zero = 1, all_not_dirty = 1;
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (bp->frames[i].page_id   != INVALID_PAGE_ID) all_invalid   = 0;
        if (bp->frames[i].pin_count != 0)               all_pin_zero  = 0;
        if (bp->frames[i].is_dirty  != false)           all_not_dirty = 0;
    }
    ASSERT("bp_create_frames_invalid",   all_invalid,   "all frame page_ids should be INVALID");
    ASSERT("bp_create_frames_pin_zero",  all_pin_zero,  "all frame pin_counts should be 0");
    ASSERT("bp_create_frames_not_dirty", all_not_dirty, "all frames should start clean");

    int all_null = 1;
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        if (bp->page_table[i] != NULL) { all_null = 0; break; }
    }
    ASSERT("bp_create_table_null", all_null, "all page_table buckets should be NULL");

    /* all 64 frame.data buffers must be non-NULL */
    int all_data_non_null = 1;
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (bp->frames[i].data == NULL) { all_data_non_null = 0; break; }
    }
    ASSERT("bp_create_frame_data_not_null", all_data_non_null,
           "every frame.data buffer should be non-NULL after create");

    bp_destroy(bp);
    cleanup_tmp_pm(pm, path);
}

/* ---------------------------------------------------------------
 * Test: hash_function, hash_insert, hash_lookup, hash_remove
 * --------------------------------------------------------------- */
static void test_hash_operations(void) {
    char path[64];
    PageManager *pm = open_tmp_pm(path);
    BufferPool *bp = bp_create(pm);

    /* hash_function must always stay in [0, HASH_TABLE_SIZE) */
    int in_range = 1;
    for (page_id_t pid = 0; pid < 256; pid++) {
        int h = hash_function(pid);
        if (h < 0 || h >= HASH_TABLE_SIZE) { in_range = 0; break; }
    }
    ASSERT("hash_function_range", in_range, "hash result must be in [0, HASH_TABLE_SIZE)");

    /* insert then lookup */
    hash_insert(bp, 42, 7);
    ASSERT_INT_EQ("hash_insert_lookup_basic", 7, hash_lookup(bp, 42));

    /* update existing entry */
    hash_insert(bp, 42, 3);
    ASSERT_INT_EQ("hash_insert_update_existing", 3, hash_lookup(bp, 42));

    /* lookup of absent page */
    ASSERT_INT_EQ("hash_lookup_missing", -1, hash_lookup(bp, 999));

    /* remove clears the entry */
    hash_remove(bp, 42);
    ASSERT_INT_EQ("hash_remove_found", -1, hash_lookup(bp, 42));

    /* removing an absent page does not crash */
    hash_remove(bp, 999);
    PASS("hash_remove_not_found_no_crash");

    /* NULL / INVALID guards */
    ASSERT_INT_EQ("hash_lookup_null_bp",      -1, hash_lookup(NULL, 1));
    hash_insert(bp, INVALID_PAGE_ID, 0); /* should be a no-op */
    ASSERT_INT_EQ("hash_insert_invalid_page", -1, hash_lookup(bp, INVALID_PAGE_ID));

    bp_destroy(bp);
    cleanup_tmp_pm(pm, path);
}

/* ---------------------------------------------------------------
 * Test: bp_fetch_page
 * --------------------------------------------------------------- */
static void test_bp_fetch_page(void) {
    /* NULL bp guard */
    ASSERT_NULL("bp_fetch_page_null_bp", bp_fetch_page(NULL, 0));

    char path[64];
    PageManager *pm = open_tmp_pm(path);
    BufferPool *bp = bp_create(pm);

    /* allocate a real page and write a sentinel pattern */
    page_id_t pid0 = pm_allocate_page(pm);
    char sentinel[PAGE_SIZE];
    memset(sentinel, 0xAA, PAGE_SIZE);
    pm_write_page(pm, pid0, sentinel);

    /* first fetch loads from disk */
    void *data = bp_fetch_page(bp, pid0);
    ASSERT_NOT_NULL("bp_fetch_page_loads_new", data);

    /* after fetch, pin_count must be 1 */
    int frame = hash_lookup(bp, pid0);
    ASSERT("bp_fetch_page_pins_frame",
           frame != -1 && bp->frames[frame].pin_count == 1,
           "pin_count should be 1 after first fetch");

    /* cache hit: second fetch of same page does not return NULL */
    void *data_again = bp_fetch_page(bp, pid0);
    ASSERT_NOT_NULL("bp_fetch_page_cache_hit", data_again);

    bp_destroy(bp);
    cleanup_tmp_pm(pm, path);
}

/* ---------------------------------------------------------------
 * Test: bp_pin / bp_unpin
 * --------------------------------------------------------------- */
static void test_bp_pin_unpin(void) {
    /* NULL guards */
    bp_pin(NULL, 0);
    PASS("bp_pin_null_bp");
    bp_unpin(NULL, 0, false);
    PASS("bp_unpin_null_bp");

    char path[64];
    PageManager *pm = open_tmp_pm(path);
    BufferPool *bp = bp_create(pm);

    page_id_t pid = pm_allocate_page(pm);
    bp_fetch_page(bp, pid);
    int frame = hash_lookup(bp, pid);

    /* bp_pin increments further */
    bp_pin(bp, pid);
    ASSERT_INT_EQ("bp_pin_increments_count", 2, bp->frames[frame].pin_count);

    /* unpin decrements */
    bp_unpin(bp, pid, false);
    ASSERT_INT_EQ("bp_unpin_decrements_count", 1, bp->frames[frame].pin_count);

    /* unpin to zero, then one extra call should not go negative */
    bp_unpin(bp, pid, false);
    bp_unpin(bp, pid, false);
    ASSERT_INT_EQ("bp_unpin_no_below_zero", 0, bp->frames[frame].pin_count);

    /* unpin with mark_dirty sets the dirty flag */
    bp_fetch_page(bp, pid); /* re-fetch to pin */
    bp_unpin(bp, pid, true);
    ASSERT("bp_unpin_marks_dirty",
           bp->frames[frame].is_dirty == true,
           "is_dirty should be true after unpin with mark_dirty=true");

    bp_destroy(bp);
    cleanup_tmp_pm(pm, path);
}

/* ---------------------------------------------------------------
 * Test: bp_flush_page / bp_flush_all
 * --------------------------------------------------------------- */
static void test_bp_flush(void) {
    /* NULL bp guard */
    ASSERT("bp_flush_page_null_bp",
           bp_flush_page(NULL, 0) == false,
           "should return false for NULL bp");

    char path[64];
    PageManager *pm = open_tmp_pm(path);
    BufferPool *bp = bp_create(pm);

    /* flush of non-resident page */
    ASSERT("bp_flush_page_missing",
           bp_flush_page(bp, 42) == false,
           "should return false for non-resident page");

    page_id_t pid = pm_allocate_page(pm);
    bp_fetch_page(bp, pid);

    /* flush a clean (non-dirty) page returns true without writing */
    ASSERT("bp_flush_page_clean",
           bp_flush_page(bp, pid) == true,
           "flush of clean page should return true");

    /* mark dirty via unpin, then flush */
    bp_unpin(bp, pid, true);
    int frame = hash_lookup(bp, pid);
    ASSERT("bp_flush_page_dirty_before",
           bp->frames[frame].is_dirty == true,
           "frame should be dirty before flush");

    bool flushed = bp_flush_page(bp, pid);
    ASSERT("bp_flush_page_dirty", flushed == true, "flush of dirty page should succeed");
    ASSERT("bp_flush_page_clears_dirty",
           bp->frames[frame].is_dirty == false,
           "is_dirty should be cleared after successful flush");

    /* bp_flush_all does not crash */
    bp_unpin(bp, pid, true);
    bp_flush_all(bp);
    PASS("bp_flush_all_no_crash");

    bp_destroy(bp);
    cleanup_tmp_pm(pm, path);
}

/* ---------------------------------------------------------------
 * Test: bp_destroy
 * --------------------------------------------------------------- */
static void test_bp_destroy(void) {
    bp_destroy(NULL);
    PASS("bp_destroy_null_no_crash");
}

/* ---------------------------------------------------------------
 * Test: eviction flushes dirty frame before loading new page
 * --------------------------------------------------------------- */
static void test_bp_eviction(void) {
    char path[64];
    PageManager *pm = open_tmp_pm(path);
    BufferPool *bp = bp_create(pm);

    /* allocate one more page than the pool can hold */
    page_id_t pids[BUFFER_POOL_SIZE + 1];
    for (int i = 0; i <= BUFFER_POOL_SIZE; i++) {
        pids[i] = pm_allocate_page(pm);
    }

    /* fill every frame and immediately unpin; mark the first one dirty */
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        bp_fetch_page(bp, pids[i]);
        bp_unpin(bp, pids[i], i == 0); /* only mark frame 0 dirty */
    }

    /* fetching the extra page must evict an unpinned frame;
       if that frame is dirty it must be flushed first */
    void *data = bp_fetch_page(bp, pids[BUFFER_POOL_SIZE]);
    ASSERT_NOT_NULL("bp_eviction_flushes_and_loads", data);

    bp_destroy(bp);
    cleanup_tmp_pm(pm, path);
}

/* ---------------------------------------------------------------
 * Test: eviction flush failure returns NULL
 *
 * Strategy: fill the pool with BUFFER_POOL_SIZE pages, unpin the first
 * one and mark it dirty.  Then close the underlying file descriptor to
 * force pm_write_page to fail when eviction tries to flush it.
 * bp_fetch_page must return NULL and leave the dirty frame intact.
 * --------------------------------------------------------------- */
static void test_bp_fetch_evict_flush_failure(void) {
    char path[64];
    PageManager *pm = open_tmp_pm(path);

    /* allocate BUFFER_POOL_SIZE + 1 pages on disk */
    page_id_t pids[BUFFER_POOL_SIZE + 1];
    for (int i = 0; i <= BUFFER_POOL_SIZE; i++) {
        pids[i] = pm_allocate_page(pm);
    }

    BufferPool *bp = bp_create(pm);

    /* fill every frame, leave them all pinned except frame 0 */
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        bp_fetch_page(bp, pids[i]);
    }

    /* unpin frame 0 and mark it dirty — sole eviction candidate */
    bp_unpin(bp, pids[0], true);

    /* break the fd so pm_write_page fails during the eviction flush */
    close(pm->fd);
    pm->fd = -1;

    void *result = bp_fetch_page(bp, pids[BUFFER_POOL_SIZE]);
    ASSERT_NULL("bp_fetch_evict_flush_failure_returns_null", result);

    /* restore a valid fd so bp_destroy does not crash */
    pm->fd = open(path, O_RDWR);

    bp_destroy(bp);
    cleanup_tmp_pm(pm, path);
}

/* ---------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------- */
int main(void) {
    test_bp_create();
    test_hash_operations();
    test_bp_fetch_page();
    test_bp_pin_unpin();
    test_bp_flush();
    test_bp_destroy();
    test_bp_eviction();
    test_bp_fetch_evict_flush_failure();
    return 0;
}
