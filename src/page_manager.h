#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE 4096
typedef int64_t page_id_t;
#define INVALID_PAGE_ID -1

typedef struct {
    int fd;                  // file descriptor
    page_id_t num_pages;     // total pages currently in the file
} PageManager;

PageManager *pm_open(const char *filename);
void pm_close(PageManager *pm);

page_id_t pm_allocate_page(PageManager *pm);
bool pm_read_page(PageManager *pm, page_id_t page_id, void *buffer);
bool pm_write_page(PageManager *pm, page_id_t page_id, const void *buffer);

#endif