#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "../src/wal.h"
#include "test_helpers.h"

const char *TEST_WAL_FILE = "test_wal.wal";

void cleanup() {
    remove(TEST_WAL_FILE);
}

int main() {
    cleanup();

    // 1. open_returns_nonnull
    WAL *wal = wal_open(TEST_WAL_FILE);
    if (wal && wal->fd >= 0) {
        PASS("open_returns_nonnull");
    } else {
        FAIL("open_returns_nonnull", "wal_open returned NULL or invalid fd");
    }
    if (wal) wal_close(wal);
    cleanup();

    // 2. open_invalid_path_returns_null
    WAL *invalid_wal = wal_open("/no/such/dir/x.wal");
    ASSERT_NULL("open_invalid_path_returns_null", invalid_wal);
    
    // 3. append_single_record_on_disk
    wal = wal_open(TEST_WAL_FILE);
    assert(wal != NULL);
    char buf[PAGE_SIZE];
    memset(buf, 'A', PAGE_SIZE);
    bool ok = wal_append(wal, 42, buf);
    assert(ok);
    wal_close(wal);
    
    int fd = open(TEST_WAL_FILE, O_RDONLY);
    if (fd < 0) {
        perror("open TEST_WAL_FILE failed");
        assert(fd >= 0);
    }
    WALRecord rec;
    ssize_t bytes = read(fd, &rec, sizeof(WALRecord));
    assert(bytes == sizeof(WALRecord));
    close(fd);
    
    if (rec.page_id == 42 && rec.seq_num == 0 && memcmp(rec.page_data, buf, PAGE_SIZE) == 0) {
        PASS("append_single_record_on_disk");
    } else {
        FAIL("append_single_record_on_disk", "record data mismatched on disk");
    }
    cleanup();

    // 4. seq_nums_are_sequential
    wal = wal_open(TEST_WAL_FILE);
    assert(wal != NULL);
    for (int i = 0; i < 3; i++) {
        memset(buf, 'A' + i, PAGE_SIZE);
        assert(wal_append(wal, 100 + i, buf));
    }
    wal_close(wal);
    
    fd = open(TEST_WAL_FILE, O_RDONLY);
    assert(fd >= 0);
    bool seq_ok = true;
    for (int i = 0; i < 3; i++) {
        bytes = read(fd, &rec, sizeof(WALRecord));
        assert(bytes == sizeof(WALRecord));
        if (rec.seq_num != i) seq_ok = false;
    }
    close(fd);
    ASSERT("seq_nums_are_sequential", seq_ok, "seq_nums were not 0, 1, 2");
    cleanup();

    // 5. next_seq_num_after_n_appends
    wal = wal_open(TEST_WAL_FILE);
    assert(wal != NULL);
    for (int i = 0; i < 5; i++) {
        assert(wal_append(wal, i, buf));
    }
    ASSERT_INT_EQ("next_seq_num_after_n_appends", 5, wal->next_seq_num);
    
    // 6. file_size_equals_n_records
    wal_close(wal);
    struct stat st;
    assert(stat(TEST_WAL_FILE, &st) == 0);
    if (st.st_size == (off_t)(5 * sizeof(WALRecord))) {
        PASS("file_size_equals_n_records");
    } else {
        FAIL("file_size_equals_n_records", "file size mismatch");
    }

    // 7. fsync_returns_true_on_open_wal
    wal = wal_open(TEST_WAL_FILE); // Reopen for fsync test
    ASSERT("fsync_returns_true_on_open_wal", wal_fsync(wal), "wal_fsync failed");
    
    // 8. append_null_wal_returns_false
    ASSERT("append_null_wal_returns_false", !wal_append(NULL, 1, buf), "accepted NULL wal");

    // 9. append_null_data_returns_false
    ASSERT("append_null_data_returns_false", !wal_append(wal, 1, NULL), "accepted NULL data");

    // 10. append_negative_page_id_returns_false
    ASSERT("append_negative_page_id_returns_false", !wal_append(wal, -1, buf), "accepted negative page_id");

    // 11. fsync_null_returns_false
    ASSERT("fsync_null_returns_false", !wal_fsync(NULL), "fsync accepted NULL wal");

    // 12. close_null_no_crash
    wal_close(NULL); // Should not crash
    PASS("close_null_no_crash");
    
    wal_close(wal); // Close the real wal from earlier
    cleanup();

    // 13. reopen_resumes_seq_num
    wal = wal_open(TEST_WAL_FILE);
    assert(wal != NULL);
    for (int i = 0; i < 3; i++) {
        assert(wal_append(wal, i, buf));
    }
    wal_close(wal);
    
    wal = wal_open(TEST_WAL_FILE);
    assert(wal != NULL);
    if (wal->next_seq_num != 3) {
        FAIL("reopen_resumes_seq_num", "next_seq_num did not resume at 3");
    } else {
        bool reopen_ok = true;
        for (int i = 0; i < 2; i++) {
            assert(wal_append(wal, i + 3, buf));
        }
        wal_close(wal);
        
        fd = open(TEST_WAL_FILE, O_RDONLY);
        assert(fd >= 0);
        for (int i = 0; i < 5; i++) {
            bytes = read(fd, &rec, sizeof(WALRecord));
            assert(bytes == sizeof(WALRecord));
            if (rec.seq_num != i) reopen_ok = false;
        }
        close(fd);
        ASSERT("reopen_resumes_seq_num", reopen_ok, "seq_nums didn't match across reopen");
    }
    cleanup();
    
    return 0;
}
