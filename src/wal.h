#ifndef WAL_H
#define WAL_H

#include "page_manager.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct BufferPool BufferPool;

typedef struct {
	// no operation type because we will implement only redo, not undo's
	int fd; // file descriptor of the WAL file
	int next_seq_num; // counter for the records
    int num_buffered;
    void *buffer;
} WAL;

typedef struct {
    page_id_t page_id;
    int seq_num;
    uint32_t checksum;
    int tx_records;
    char page_data[PAGE_SIZE];   // the serialized node content
} WALRecord;

WAL *wal_open(const char *filename); // open or create the WAL file
bool wal_append(WAL *wal, page_id_t page_id, void *page_data);
bool wal_fsync(WAL *wal);
void wal_close(WAL *wal);

bool wal_recover(WAL *wal, BufferPool *bp);
void wal_clear(WAL *wal);

bool wal_checkpoint(BufferPool *bp);
bool wal_contains_page(WAL *wal, page_id_t page_id);

#endif
