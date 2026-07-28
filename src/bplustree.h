#ifndef BPLUSTREE_H
#define BPLUSTREE_H

#include <stdbool.h>

#define MAX_KEYS 4

typedef struct Node {
    struct Node *parent;
	int keys[MAX_KEYS];
	int num_keys;
	bool is_leaf;

	union {
        struct {
            void *values[MAX_KEYS];   
            struct Node *next;    
        } leaf;
        
        struct {
            struct Node *children[MAX_KEYS + 1];
        } inner;
    } data;
} Node;

typedef struct Tree {
	Node *root;
} Tree;

Tree *createTree();
Node *insert(Node *root, int key, void *value);
void *search(Node *root, int key);
Node *deleteNode(Node *root, int key);
Node *create_leaf_node();
Node *create_inner_node();
void freeNode(Node *node);
int find_key_in_node(Node *node, int key);
void *find_child_index(Node *inner_node, int key);
void insert_into_leaf_sorted(Node *leaf, int key, void *value);
Node *split_leaf(Node *leaf);
Node *split_inner_node(Node *node);
Node *insert_into_parent(Node *parent, Node *left_child, int key, Node *right_child);
Node *remove_from_leaf(Node *leaf, int key);

#endif