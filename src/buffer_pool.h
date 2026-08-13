#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include "page_manager.h"
#include "wal.h"
#include <stdbool.h>

#define BUFFER_POOL_SIZE 64	// number of frames (pages) held in memory
#define HASH_TABLE_SIZE 127

typedef struct HashNode {
	page_id_t page_id;
	int frame_idx; // where the page is in the frames array
	struct HashNode *next; // pointer to the next node if a collision happens
} HashNode;

typedef struct {
	page_id_t page_id;
	void *data; // PAGE_SIZE bytes
	bool is_dirty;
	int pin_count;
	int last_time_used;
} Frame;

typedef struct {
	PageManager *pm;
	WAL *wal;
	Frame frames[BUFFER_POOL_SIZE];
	int global_time;
	HashNode *page_table[HASH_TABLE_SIZE];
} BufferPool;

BufferPool *bp_create(PageManager *pm, const char*filename);
void bp_destroy(BufferPool *bp); // flushes all dirty frames, frees memory

/*
 * Raw-bytes contract for Frame.data / bp_fetch_page:
 *
 *   - Frame.data is a raw PAGE_SIZE byte buffer.  The buffer pool never
 *     calls serialize_node / deserialize_node internally.
 *
 *   - After bp_fetch_page returns, the caller must call
 *     deserialize_node(data) to obtain a usable Node*.  That Node* is
 *     heap-allocated and the CALLER is responsible for freeing it.
 *
 *   - Before calling bp_unpin(bp, page_id, true  [mark_dirty=true]), the
 *     caller must write the updated Node back into the frame buffer with
 *     serialize_node(node, data).
 *
 *   - The returned void* is only valid while pin_count > 0 for that page.
 *     Once unpinned the frame may be evicted at any time and the pointer
 *     becomes stale.
 */
void *bp_fetch_page(BufferPool *bp, page_id_t page_id); // returns pointer to in-memory frame data
void bp_pin(BufferPool *bp, page_id_t page_id);
void bp_unpin(BufferPool *bp, page_id_t page_id, bool mark_dirty);
bool bp_flush_page(BufferPool *bp, page_id_t page_id);
void bp_flush_all(BufferPool *bp);

int hash_function(page_id_t page_id); // the hashing function
void hash_insert(BufferPool *bp, page_id_t page_id, int frame_index);
void hash_remove(BufferPool *bp, page_id_t page_id);
int hash_lookup(BufferPool *bp, page_id_t page_id); // O(1) page looking function

#endif