#ifndef BPLUSTREE_H
#define BPLUSTREE_H

#include <stdbool.h>

#include <stdint.h>
#include "page_manager.h" // for page_id_t and INVALID_PAGE_ID
#include "buffer_pool.h"

#define MAX_KEYS 4

typedef struct Node {
	page_id_t page_id; 
	page_id_t parent_id;
	int num_keys;
	bool is_leaf;
	int keys[MAX_KEYS];

	union {
        struct {
            int64_t values[MAX_KEYS];   
            page_id_t next_id;    
        } leaf;
        
        struct {
            page_id_t children[MAX_KEYS + 1];
        } inner;
    } data;
} Node;


typedef struct Tree {
	page_id_t root_id;
} Tree;

page_id_t allocate_node_page(BufferPool *bp);

Tree *createTree();
page_id_t insert(BufferPool *bp, page_id_t root_id, int key, void *value);
void *search(BufferPool *bp, page_id_t root_id, int key);
page_id_t deleteNode(BufferPool *bp, page_id_t root_id, int key);
page_id_t create_leaf_node(BufferPool *bp);
page_id_t create_inner_node(BufferPool *bp);
int find_key_in_node(Node *node, int key);
void insert_into_leaf_sorted(Node *leaf, int key, void *value);
Node *split_leaf(BufferPool *bp, Node *leaf);
Node *split_inner_node(BufferPool *bp, Node *node);
page_id_t insert_into_parent(BufferPool *bp, page_id_t parent_id, page_id_t left_child, int key, page_id_t right_child);
bool try_borrow_from_left_sibling(BufferPool *bp, Node *node, Node *parent, int index);
bool try_borrow_from_right_sibling(BufferPool *bp, Node *node, Node *parent, int index);
Node *merge_with_sibling(BufferPool *bp, Node *node, Node *sibling, Node *parent, int index);
Node *remove_from_parent(BufferPool *bp, Node *parent, int index);
bool validate_tree(BufferPool *bp, page_id_t root_id);
bool key_check_less(BufferPool *bp, page_id_t root_id, int n);
bool key_check_greater_eq(BufferPool *bp, page_id_t root_id, int n);
bool range_search(BufferPool *bp, page_id_t root_id, int start, int end, int result_keys[], void *result_values[], int *num_res);

#endif