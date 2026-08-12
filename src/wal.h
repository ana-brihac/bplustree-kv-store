#ifndef WAL_H
#define WAL_H

#include "page_manager.h"
#include "buffer_pool.h"
#include <stdbool.h>

typedef struct {
	// no operation type because we will implement only redo, not undo's
	int fd; // file descriptor of the WAL file
	int next_seq_num; // counter for the records
} WAL;

typedef struct {
    page_id_t page_id;
    int seq_num;
    char page_data[PAGE_SIZE];   // the serialized node content
} WALRecord;

WAL *wal_open(char *filename); // open or create the WAL file
bool wal_append(WAL *wal, page_id_t page_id, void *page_data);
bool wal_fsync(WAL *wal);
void wal_close(WAL *wal);

#endif