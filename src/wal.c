#include <stdio.h>
#include <stdlib.h>

#include "wal.h"

WAL *wal_open(char *filename) {
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

	return new_wal;
}

bool wal_append(WAL *wal, page_id_t page_id, void *page_data) {
	if (!wal || !page_data || page_id < 0) {
		return false;
	}

	WALRecord *new_rec;

	new_rec->page_id = page_id;
	new_rec->seq_num = new_rec->next_seq_num;
	wal->next_seq_num ++;
	memcpy(new_rec->page_data, page_data, PAGE_SIZE);

	ssize_t bytes_written = write(wal->fd, &record, sizeof(WALRecord));
	
	if (bytes_written != sizeof(WALRecord)) {
		return false;  // write failed or was incomplete
	}

	return true;
}

bool wal_fsync(WAL *wal) { 
	if (!wal) {
		return false;
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
	free(wal);
}