#include <stdio.h>
#include <bplustree.h>

Tree *createTree() {
	Tree newTree = malloc(sizeof(struct));
	newTree.root = null;

	return newTree;
}

Node *create_leaf_node() {

}

Node *create_inner_node() {

}

Node *insert(Node *root, int key, void *value) {

}

void *search(Node *node, int key) {
	if (node->is_leaf) { // if leaf, going through its keys to see if our target key is there
		for (int i = 0; i < node->num_keys; i ++) {
			if (node->keys[i] == key) { // foudn the key, return the node
				return node;
			}
		}

		return NULL; // key not found in the leaf, so it doesn't exist in the tree
	} else { // if inner, checking the children which keys are smaller than our key
		for (int i = 0; i < node->num_keys; i ++) {
			if (key < node->keys[i]) { 
				return search(node->data.inner.children[i], key);
			}
		}

		return search(children[num_keys], key); // recursive search if we don't find the rigth node
		// the very last node on the far right is the safest next point 
	}

	return NULL; 
}

Node *deleteNode(Node *root, int key) {
	Node *delNode = search(root, key); // finding the node after the key

	if (!delNode) { // checking if we have a node with this key in our tree
		return root;
	} 

	for (int i = 0; i < delNode->num_keys; i ++) {
		if (delNode->keys[i] == key) {
			free(delNode->data.leaf.values[i]);
			
			for (int j = i; j < delNode->num_keys - 1; j ++) { 
				// we are removing the key and value from the node
				// we are shifting the values to the left
				delNode->data.leaf.values[j] = delNode->data.leaf.values[j + 1];
				delNode->keys[j] = delNode->keys[j + 1];
			}

			delNode->num_keys --;

			// after deleting the key we need to check if the tree is still balanced
			// but in my courses I read that we don t need to check for balance right away
			// we are doing this verifications at an interval, because we can have multiple deletions 
			// and multiple insertions, so maybe we don t need to balance the tree everytime
			
			break;
		}
	}

	return root;
}

void freeNode(Node *node) {
	if (!node) { // checking if we have something to free
		return;
	}

	if (node->is_leaf) { // checking the node type
		free(node->data.leaf.values); // if is leaf, freeing the values pointer
	} else { //if is inner, freeing every childer node of the inner node
		for (int x = 0; x < node->num_keys + 1; x++) {
			free_node(node->data.inner.children[x]);
		}

		free(node); // freeing the node at the end
	}

	return;
}

int main() {

	return 0;
}