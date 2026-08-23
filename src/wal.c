#if defined(__unix__) || defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "compat.h"

#include "wal.h"
#include "buffer_pool.h"

#ifndef O_BINARY
#define O_BINARY 0 /* no-op on POSIX; required on Windows so raw WAL record bytes aren't CRLF-translated */
#endif

uint32_t calculate_checksum(const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t checksum = 0;
    for (size_t i = 0; i < size; i++) {
        checksum = (checksum << 5) + checksum + bytes[i]; // simple djb2-like hash
    }
    return checksum;
}

WAL *wal_open(const char *filename) {
    int fd = open(filename, O_RDWR | O_CREAT | O_BINARY, 0666);
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
    if (!new_wal->buffer) {
        close(fd);
        free(new_wal);
        return NULL;
    }
    return new_wal;
}

bool wal_append(WAL *wal, page_id_t page_id, void *page_data) {
    if (!wal || !page_data || page_id < 0) {
        return false;
    }
    if (wal->num_buffered >= 64) {
        return false; // Transaction too large!
    }

    // Check if the page is already in the buffer
    for (int i = 0; i < wal->num_buffered; i++) {
        WALRecord *rec = &((WALRecord*)wal->buffer)[i];
        if (rec->page_id == page_id) {
            // Update the existing record
            memcpy(rec->page_data, page_data, PAGE_SIZE);
            rec->checksum = calculate_checksum(page_data, PAGE_SIZE);
            return true;
        }
    }

    WALRecord *new_rec = &((WALRecord*)wal->buffer)[wal->num_buffered];
    new_rec->page_id = page_id;
    new_rec->seq_num = wal->next_seq_num;
    new_rec->checksum = calculate_checksum(page_data, PAGE_SIZE);
    wal->next_seq_num++;
    memcpy(new_rec->page_data, page_data, PAGE_SIZE);
    wal->num_buffered++;
    return true;
}

bool wal_fsync(WAL *wal) { 
    if (!wal) {
        return false;
    }
    if (wal->num_buffered > 0) {
        WALRecord *records = (WALRecord *)wal->buffer;
        for (int i = 0; i < wal->num_buffered; i++) {
            records[i].tx_records = wal->num_buffered;
        }
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
    wal->next_seq_num = 0; // Temporarily reset to 0 to match records sequentially from start
    WALRecord record;
    WALRecord tx_buffer[64];
    int tx_count = 0;
    int expected_tx = 0;

    // reading the records from the recovery file
    while (read(wal->fd, &record, sizeof(WALRecord)) == sizeof(WALRecord)) {
        uint32_t actual_checksum = calculate_checksum(record.page_data, PAGE_SIZE);
        if (actual_checksum != record.checksum) {
            printf("Torn write detected for seq=%d (expected %u, got %u). Stopping recovery.\n", record.seq_num, record.checksum, actual_checksum);
            break;
        }
        if (record.seq_num != wal->next_seq_num) {
            // End of valid log
            break;
        }
        if (tx_count == 0) {
            expected_tx = record.tx_records;
        } else if (record.tx_records != expected_tx) {
            printf("Transaction size mismatch detected. Stopping recovery.\n");
            break;
        }
        tx_buffer[tx_count++] = record;
        wal->next_seq_num++;

        if (tx_count == expected_tx) {
            // Apply the full transaction
            for (int i = 0; i < tx_count; i++) {
                bool res = pm_write_page(bp->pm, tx_buffer[i].page_id, tx_buffer[i].page_data);
                if (!res) {
                    return false;
                }
            }
            tx_count = 0;
        }
    }
    return true;
}

void wal_clear(WAL *wal) {
    if (!wal) {
        return;
    }
    ftruncate(wal->fd, 0);
    /* ftruncate() does not move the fd's write offset. Without this seek,
     * the offset stays wherever wal_recover()'s read() loop left it (e.g.
     * just after scanning N bytes of a recovered log), so the next
     * wal_fsync() would write its records starting at that stale offset
     * instead of 0 -- leaving a zero-byte hole at the head of the file.
     * A zeroed WALRecord checksums to 0 and reads back as seq_num 0, so a
     * later wal_recover() mistakes that hole for one bogus transaction,
     * advances past it, and then rejects the real seq_num-0 record that
     * follows as out of order -- discarding every record written after
     * the hole. */
    lseek(wal->fd, 0, SEEK_SET);
    wal->next_seq_num = 0;
}

bool wal_checkpoint(BufferPool *bp) {
    if (!bp || !bp->wal) {
        return false;
    }
    // flush WAL to disk first so that no buffered records are lost if we crash before DB fsync
    wal_fsync(bp->wal);
    // flush all dirty pages from the buffer pool to the main DB file
    if (!bp_flush_all(bp)) {
        // a page failed to make it to disk -- the WAL still holds the only
        // durable copy of that write, so it must NOT be truncated below
        return false;
    }
    // ensure the OS has written all those flushed pages to disk before we clear the WAL!
    if (fsync(bp->pm->fd) < 0) {
        return false;
    }
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

bool wal_contains_page(WAL *wal, page_id_t page_id) {
    if (!wal) return false;
    for (int i = 0; i < wal->num_buffered; i++) {
        WALRecord *rec = (WALRecord *)wal->buffer + i;
        if (rec->page_id == page_id) {
            return true;
        }
    }
    return false;
}
