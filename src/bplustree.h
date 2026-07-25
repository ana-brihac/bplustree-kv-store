#ifndef BPLUSTREE_H
#define BPLUSTREE_H

#define MAX_KEYS 4

typedef struct Node {
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

#endif