#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "page_manager.h"

PageManager *pm_open(const char *filename) {
	if (!filename) { // don't have a valid name for a file
		return NULL;
	}

	// we allocate a new page manager
	PageManager *pm = malloc(sizeof(PageManager)); 

	if (!pm) { // failed to allocate memory
		return NULL;
	}

	pm->fd = open(filename, O_RDWR | O_CREAT, 0666);

	if (pm->fd < 0) { // failed to open the file
		free(pm);

		return NULL;
	}

	// we need to calculate the number of pages in the "filename"
	// we need to use the PAGE_SIZE for that to devide by it 
	struct stat st;

	if (!stat(filename, &st)) { 
		off_t fsize = st.st_size;
		pm->num_pages = fsize / PAGE_SIZE;

		return pm;
	} else { // if we can not get the filesize, clean up and return null
		close(pm->fd);
		free(pm);

		return NULL;
	}
}

void pm_close(PageManager *pm) {
	if (!pm) { // don't have  avalid pagemanager pointer
		return;
	}

	close(pm->fd);
	free(pm);
}

page_id_t pm_allocate_page(PageManager *pm) {
	off_t offset = pm->num_pages * PAGE_SIZE; // calculate the offset for the new page

	lseek(pm->fd, offset, SEEK_SET); // seek that offset
	
	char buffer[PAGE_SIZE] = {0}; // creating a blank page_size buffer

	ssize_t bytes_written = write(pm->fd, buffer, PAGE_SIZE);

	if (bytes_written != PAGE_SIZE) {
		/* Disk full or I/O error: do not increment num_pages, the page is invalid. */
		return INVALID_PAGE_ID;
	}

	pm->num_pages ++; // increment the num of pages

	return pm->num_pages - 1; // return the new page id
}

bool pm_read_page(PageManager *pm, page_id_t page_id, void *buffer) {
	if (page_id >= pm->num_pages) { // check if th epage actually exists in the page manager
        return false;
    }

	off_t offset = page_id * PAGE_SIZE; // calculate the offset for the page location

	off_t seek_ret = lseek(pm->fd, offset, SEEK_SET); // seek that offset

	if (seek_ret == (off_t)-1) {
		return false;
	}

	ssize_t bytes_read = read(pm->fd, buffer, PAGE_SIZE); // read the page in the buffer

	if (bytes_read != PAGE_SIZE) {
		return false;
	}

	return true;
}

bool pm_write_page(PageManager *pm, page_id_t page_id, const void *buffer) {
        if (page_id >= pm->num_pages) { 
            // Grow the file if necessary (important for WAL recovery!)
            off_t offset = (page_id + 1) * PAGE_SIZE - 1;
            lseek(pm->fd, offset, SEEK_SET);
            write(pm->fd, "", 1); // write 1 byte to extend file
            pm->num_pages = page_id + 1;
        }

        off_t offset = page_id * PAGE_SIZE; // calculate the offset for the new page

	off_t seek_ret = lseek(pm->fd, offset, SEEK_SET); // seek that offset

	if (seek_ret == (off_t)-1) {
		return false;
	}

	ssize_t bytes_write = write(pm->fd, buffer, PAGE_SIZE); // write the buffer to disk

	if (bytes_write != PAGE_SIZE) {
		return false;
	}

	return true;
}