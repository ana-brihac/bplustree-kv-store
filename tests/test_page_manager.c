#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/page_manager.h"

int main() {
    const char *db_filename = "test_pm.db";
    
    // Clean up previous test runs if any
    remove(db_filename);

    // 1. Open
    PageManager *pm = pm_open(db_filename);
    assert(pm != NULL);

    // 2. Allocate
    page_id_t pid = pm_allocate_page(pm);
    assert(pid == 0);

    // 3. Write data
    char write_buf[PAGE_SIZE];
    memset(write_buf, 0xAB, PAGE_SIZE);
    strcpy(write_buf, "Hello B+Tree Storage!");
    bool w_ok = pm_write_page(pm, pid, write_buf);
    assert(w_ok == true);

    // 4. Close
    pm_close(pm);

    // 5. Reopen
    pm = pm_open(db_filename);
    assert(pm != NULL);
    assert(pm->num_pages == 1);

    // 6. Read back
    char read_buf[PAGE_SIZE];
    bool r_ok = pm_read_page(pm, pid, read_buf);
    assert(r_ok == true);

    // 7. Verify
    assert(memcmp(write_buf, read_buf, PAGE_SIZE) == 0);

    // Cleanup
    pm_close(pm);
    remove(db_filename);

    printf("Page manager test passed.\n");
    return 0;
}
