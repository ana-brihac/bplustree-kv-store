#include <stdint.h>
#include <stddef.h>

uint32_t calculate_checksum(const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t checksum = 0;
    for (size_t i = 0; i < size; i++) {
        checksum = (checksum << 5) + checksum + bytes[i]; // simple djb2-like hash
    }
    return checksum;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "wal.h"
#include "buffer_pool.h"

WAL *wal_open(const char *filename) {
	int fd = open(filename, O_APPEND | O_RDWR | O_CREAT, 0666);

	if (fd < 0) {
		return NULL;
	}

	WAL *new_wal = malloc(sizeof(WAL));

	if (!new_wal) {
		close(fd);
		return NULL;
	}

	off_t file_size = lseek(fd, 0, SEEK_END);

	if (file_size) {
		new_wal->next_seq_num = file_size / sizeof(WALRecord);
	} else {
		new_wal->next_seq_num = 0;
	}

	new_wal->fd = fd;
    new_wal->num_buffered = 0;
    new_wal->buffer = malloc(64 * sizeof(WALRecord)); // 64 pages per transaction max

	return new_wal;
}

bool wal_append(WAL *wal, page_id_t page_id, void *page_data) {
	if (!wal || !page_data || page_id < 0) {
		return false;
	}

    if (wal->num_buffered >= 64) {
        return false; // Transaction too large!
    }

	WALRecord *new_rec = &((WALRecord*)wal->buffer)[wal->num_buffered];

	new_rec->page_id = page_id;
	new_rec->seq_num = wal->next_seq_num;
        new_rec->checksum = calculate_checksum(page_data, PAGE_SIZE);
	wal->next_seq_num ++;
	memcpy(new_rec->page_data, page_data, PAGE_SIZE);
    
    wal->num_buffered ++;

	return true;
}

bool wal_fsync(WAL *wal) { 
	if (!wal) {
		return false;
	}

    if (wal->num_buffered > 0) {
        ssize_t bytes_to_write = wal->num_buffered * sizeof(WALRecord);
                ssize_t bytes_written = write(wal->fd, wal->buffer, bytes_to_write);
        
        if (bytes_written != bytes_to_write) {
            perror("wal_fsync write failed");
            return false;
        }
        wal->num_buffered = 0;
    }

	int res = fsync(wal->fd); // putting the data from the file on disk

	if (res < 0) {
		return false;
	}

	return true;
}

void wal_close(WAL *wal) { 
	if (!wal) {
        return;
    }

	wal_fsync(wal);

	close(wal->fd);
    free(wal->buffer);
	free(wal);
}

bool wal_recover(WAL *wal, BufferPool *bp) {
	if (!wal || !bp) {
		return false;
	}

	lseek(wal->fd, 0, SEEK_SET); // resetting the offset to the start of the file

	WALRecord record;

	// reading the records from the recovery file
	while (read(wal->fd, &record, sizeof(WALRecord)) == sizeof(WALRecord)) {
		uint32_t actual_checksum = calculate_checksum(record.page_data, PAGE_SIZE);
		if (actual_checksum != record.checksum) {
			printf("Torn write detected for seq=%d (expected %u, got %u). Stopping recovery.\n", record.seq_num, record.checksum, actual_checksum);
			break;
		}

		// writing the data 
		bool res = pm_write_page(bp->pm, record.page_id, record.page_data);

		if (!res) {
			return false;
		}
	}

	return true;
}

void wal_clear(WAL *wal) {
	if (!wal) {
		return;
	}

	ftruncate(wal->fd, 0);
	wal->next_seq_num = 0;
}

bool wal_checkpoint(BufferPool *bp) {
    if (!bp || !bp->wal) {
        return false;
    }

    // flush all dirty pages from the buffer pool to the main DB file
    bp_flush_all(bp);
    
    // ensure the OS has written all those flushed pages to disk before we clear the WAL!
    fsync(bp->pm->fd);

    // truncate the WAL file to 0 byte
    if (ftruncate(bp->wal->fd, 0) < 0) {
        return false;
    }

    // reset the file offset back to the beginning
    if (lseek(bp->wal->fd, 0, SEEK_SET) < 0) {
        return false;
    }

    // reset the sequence counter
    bp->wal->next_seq_num = 0;

    return true;
}