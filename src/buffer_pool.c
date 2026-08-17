#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "buffer_pool.h"

BufferPool *bp_create(PageManager *pm, WAL *wal) {
	// we need to allocate memory for the new bp
	BufferPool *new_bp = malloc(sizeof(BufferPool));

	if (!new_bp) { // checking if we have memory
		return NULL;
	}
	
	new_bp->wal = wal;

	// setting the bp variable
	new_bp->pm = pm;
	new_bp->global_time = 0;

	// allocating memory for the frames and initiating them
	for (int i = 0; i < BUFFER_POOL_SIZE; i ++) {
		new_bp->frames[i].data = malloc(PAGE_SIZE);

		if (!new_bp->frames[i].data) { // failed to allocate data for the frame, clean up
			for (int j = 0; j < i; j++) {
				free(new_bp->frames[j].data);
			}
			free(new_bp);
			return NULL;
		}

		new_bp->frames[i].page_id = INVALID_PAGE_ID;
		new_bp->frames[i].last_time_used = 0;
		new_bp->frames[i].pin_count = 0;
		new_bp->frames[i].is_dirty = false;
	}

	// allocating memory for the hash table and initiating it
	for (int i = 0; i < HASH_TABLE_SIZE; i ++) {
		new_bp->page_table[i] = NULL;
	}

	return new_bp;
}

void bp_destroy(BufferPool *bp) {
	if (!bp) { // checking if we have a valid bp
		return;
	}

	bp_flush_all(bp); // flushing the dirty pages

	for (int i = 0; i < BUFFER_POOL_SIZE; i ++) {
		free(bp->frames[i].data);
	}
	for (int i = 0; i < HASH_TABLE_SIZE; i++) {
		HashNode *current = bp->page_table[i];
		
		while (current != NULL) {
			HashNode *next = current->next;
			free(current);
			current = next;
		}
	}

	free(bp);
}

void *bp_fetch_page(BufferPool *bp, page_id_t page_id) {
	if (!bp) {
		return NULL;
	}

	int frame_idx = hash_lookup(bp, page_id); 
    
    if (frame_idx == -1) { // checking if we found the right page in memory
		// the page is not in RAM, we need to fetch it
		int time_evict = 1000000000;
		int k = -1;

		for (int i = 0; i < BUFFER_POOL_SIZE; i ++) { // checking for an empty frame
			if (bp->frames[i].page_id == INVALID_PAGE_ID) {
				k = i;
				break;
			}

			if (bp->frames[i].pin_count == 0) {
				if (bp->wal && wal_contains_page(bp->wal, bp->frames[i].page_id)) {
					continue;
				}
				if (bp->frames[i].last_time_used < time_evict) {
					k = i; 
					time_evict = bp->frames[i].last_time_used;
				}
			}
		}

		if (k == -1) {
			return NULL;
		}

		// we did not found an empty frame, we need to evict the last used
		if (bp->frames[k].is_dirty) {
			if (!bp_flush_page(bp, bp->frames[k].page_id)) { // can't evict if we can't flush
				return NULL;
			}
		}

		hash_remove(bp, bp->frames[k].page_id);
		pm_read_page(bp->pm, page_id, bp->frames[k].data);
		hash_insert(bp, page_id, k);

		bp->frames[k].page_id = page_id;
		bp->frames[k].pin_count = 1;
		bp->frames[k].is_dirty = false;
		bp->frames[k].last_time_used = bp->global_time;
		bp->global_time++;

		return bp->frames[k].data;
    } else {
		bp_pin(bp, page_id);

		bp->frames[frame_idx].last_time_used = bp->global_time;
        bp->global_time++;

		return bp->frames[frame_idx].data;
	}
}

void bp_pin(BufferPool *bp, page_id_t page_id) {
	if (!bp) {
		return;
	}

	int frame_idx = hash_lookup(bp, page_id); 
    
    if (frame_idx != -1) { // checking if we found the right page in memory
        bp->frames[frame_idx].pin_count++;
    }
}

void bp_unpin(BufferPool *bp, page_id_t page_id, bool mark_dirty) {
	if (!bp) {
		return;
	}

	int frame_idx = hash_lookup(bp, page_id); 

	if (frame_idx != -1) { // checking if we found the right page
		if (mark_dirty) { // if the caller modified it, write to WAL
			wal_append(bp->wal, page_id, bp->frames[frame_idx].data);
			bp->frames[frame_idx].is_dirty = true;
			
			// trigger automatic checkpoint if WAL grows too large
		}

		if (bp->frames[frame_idx].pin_count > 0) { // decrementing the pin count
			bp->frames[frame_idx].pin_count--;
		}
	}
}

bool bp_flush_page(BufferPool *bp, page_id_t page_id) {
	if (!bp) {
		return false;
	}

	int frame_idx = hash_lookup(bp, page_id); 

	if (frame_idx == -1) { // checking if we found the right page in memory
		return false;
	}

	if (bp->frames[frame_idx].is_dirty) {
		// attempt to write to disk and capture the result
        bool success = pm_write_page(bp->pm, page_id, bp->frames[frame_idx].data);
        
        // only mark it clean if the write actually worked
        if (success) {
            bp->frames[frame_idx].is_dirty = false;
        }

		return success;
	}

	return true;
}

void bp_flush_all(BufferPool *bp) {
	if (!bp) {
		return;
	}

	for (int i = 0; i < BUFFER_POOL_SIZE; i ++) {
		if (bp->frames[i].is_dirty) { // flushing only the dirty pages
			bp_flush_page(bp, bp->frames[i].page_id);
		}
	}
}

int hash_function(page_id_t page_id) {
	return (int)(page_id % HASH_TABLE_SIZE);
}

void hash_insert(BufferPool *bp, page_id_t page_id, int frame_index) {
	if (!bp || page_id == INVALID_PAGE_ID) {
        return;
    }

	int index = hash_function(page_id);
	HashNode *current = bp->page_table[index];

	while (current != NULL) {
		if (current->page_id == page_id) {
			current->frame_idx = frame_index;

			return;
		}

		current = current->next;
	}

	HashNode *new_node = malloc(sizeof(HashNode));

	if (!new_node) {
		return; // out of memory safety check
	}

	new_node->page_id = page_id;
	new_node->frame_idx = frame_index;

	// insert at the head of the linked list
	new_node->next = bp->page_table[index]; // point new node to the old head
	bp->page_table[index] = new_node;
}

void hash_remove(BufferPool *bp, page_id_t page_id) {
	if (!bp || page_id == INVALID_PAGE_ID) {
        return;
    }

	int index = hash_function(page_id);

	HashNode *current = bp->page_table[index];
	HashNode *prev = NULL;

	while (current != NULL) {
		if (current->page_id == page_id) {
			if (!prev) { // the next becomes the new head
				bp->page_table[index] = current->next;
			} else {
                prev->next = current->next; // we change the next for continuancy 
            }

			free(current);
			return;
		}

		prev = current;
		current = current->next;
	}
}

int hash_lookup(BufferPool *bp, page_id_t page_id) {
	if (!bp || page_id == INVALID_PAGE_ID) {
		return -1;
	}

	// calculate the hash index
	int index = hash_function(page_id);

	// the first node of the index list
	HashNode *current = bp->page_table[index];

	// looking for the right page
	while (current != NULL) {
		if (current->page_id == page_id) {
			return current->frame_idx;
		}

		current = current->next;
	}

	// if we did not found it, return -1
	return -1;
}