static void test_bp_unpin_wal_logging(void) {
    char pm_path[64];
    char wal_path[64];
    PageManager *pm = open_tmp_pm(pm_path);
    strcpy(wal_path, "/tmp/wal_test_XXXXXX");
    int fd = mkstemp(wal_path);
    close(fd);
    
    BufferPool *bp = bp_create(pm, wal_path);
    
    page_id_t pid = pm_allocate_page(pm);
    void *data = bp_fetch_page(bp, pid);
    
    memset(data, 0xBB, PAGE_SIZE);
    
    bp_unpin(bp, pid, false);
    
    struct stat st;
    stat(wal_path, &st);
    ASSERT("bp_unpin_clean_no_wal", st.st_size == 0, "WAL should be empty if mark_dirty=false");
    
    bp_fetch_page(bp, pid);
    bp_unpin(bp, pid, true);
    
    stat(wal_path, &st);
    ASSERT("bp_unpin_dirty_wal", st.st_size > 0, "WAL should have data if mark_dirty=true");
    
    int wal_fd = open(wal_path, O_RDONLY);
    WALRecord record;
    read(wal_fd, &record, sizeof(WALRecord));
    close(wal_fd);
    
    ASSERT_INT_EQ("bp_unpin_wal_page_id", pid, record.page_id);
    ASSERT("bp_unpin_wal_data_match", record.page_data[0] == (char)0xBB, "WAL data should match modified page");
    
    bp_destroy(bp);
    cleanup_tmp_pm(pm, pm_path);
    unlink(wal_path);
}
