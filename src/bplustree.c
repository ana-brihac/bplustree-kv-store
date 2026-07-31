#include <stdio.h>
#include <stdlib.h>
#include "bplustree.h"

Tree *createTree() {
	Tree *newTree = malloc(sizeof(Tree));
	if (newTree) {
		newTree->root = NULL;
	}

	return newTree;
}

Node *create_leaf_node() {
	Node *new_node = malloc(sizeof(struct Node));
	if (!new_node) return NULL;

	new_node->is_leaf = true;
	new_node->num_keys = 0;
	new_node->parent = NULL; // no parent yet
	for (int i = 0; i < MAX_KEYS; i++) {
		new_node->data.leaf.values[i] = NULL;
	}
	new_node->data.leaf.next = NULL;

	return new_node;
}

Node *create_inner_node() {
	Node *new_node = malloc(sizeof(struct Node));
	if (!new_node) return NULL;

	new_node->is_leaf = false;
	new_node->num_keys = 0;
	new_node->parent = NULL; // no parent yet
	for (int i = 0; i < MAX_KEYS + 1; i++) {
		new_node->data.inner.children[i] = NULL;
	}

	return new_node;
}

Node *insert(Node *root, int key, void *value) {
	// case 1: empty tree, create the first leaf
	if (!root) {
		root = create_leaf_node();
		insert_into_leaf_sorted(root, key, value);
		return root;
	}

	// step 1: traverse down to the correct leaf
	Node *leaf = root;
	while (!leaf->is_leaf) {
		int i = 0;
		while (i < leaf->num_keys && key > leaf->keys[i]) { // finding the correct child
			i++;
		}
		leaf = leaf->data.inner.children[i];
	}

	// step 2: leaf has space, just insert
	if (leaf->num_keys < MAX_KEYS) {
		insert_into_leaf_sorted(leaf, key, value);
		return root;
	}

	// step 3: leaf is full, split first then insert into the correct half
	Node *new_leaf = split_leaf(leaf);
	int guidepost = new_leaf->keys[0]; // smallest key of the right leaf goes up

	if (key >= guidepost) { // new key belongs in the right (new) leaf
		insert_into_leaf_sorted(new_leaf, key, value);
	} else { // new key belongs in the left (original) leaf
		insert_into_leaf_sorted(leaf, key, value);
	}

	// step 4: push guidepost key up into the parent
	Node *new_root = insert_into_parent(leaf->parent, leaf, guidepost, new_leaf);

	if (!leaf->parent) { // we split the root, tree grew by one level
		new_leaf->parent = new_root;
		leaf->parent = new_root;
		return new_root;
	}

	return root;
}

void *search(Node *node, int key) {
	if (!node) {
		return NULL;
	}
	if (node->is_leaf) { // if leaf, going through its keys to see if our target key is there
		for (int i = 0; i < node->num_keys; i ++) {
			if (node->keys[i] == key) { // found the key, return the value
				return node->data.leaf.values[i];
			}
		}

		return NULL; // key not found in the leaf, so it doesn't exist in the tree
	} else { // if inner, checking the children which keys are smaller than our key
		for (int i = 0; i < node->num_keys; i ++) {
			if (key < node->keys[i]) {
				return search(node->data.inner.children[i], key);
			}
		}

		return search(node->data.inner.children[node->num_keys], key); // recursive search if we don't find the rigth node
		// the very last node on the far right is the safest next point
	}

	return NULL;
}

Node *deleteNode(Node *root, int key) {
	Node *delNode = root;
	while (delNode && !delNode->is_leaf) {
		int i = 0;
		while (i < delNode->num_keys && key >= delNode->keys[i]) {
			i++;
		}
		delNode = delNode->data.inner.children[i];
	}

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
			delNode->keys[delNode->num_keys] = 0; // clearing the stale slot left after the shift
			delNode->data.leaf.values[delNode->num_keys] = NULL;

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

	if (!node->is_leaf) { //if is inner, freeing every childer node of the inner node
		for (int x = 0; x < node->num_keys + 1; x++) {
			freeNode(node->data.inner.children[x]);
		}
	}

	free(node); // freeing the node at the end
}

void insert_into_leaf_sorted(Node *leaf, int key, void *value) {
	int i = 0;

	while (i < leaf->num_keys && key > leaf->keys[i]) {  // finding the correct index
		i++;
	}

	if (i < leaf->num_keys && leaf->keys[i] == key) { // rejecting duplicate keys
		return;
	}

	for (int j = leaf->num_keys; j > i; j--) {
		leaf->keys[j] = leaf->keys[j - 1];
		leaf->data.leaf.values[j] = leaf->data.leaf.values[j - 1];
	}

	leaf->keys[i] = key; // insert the key
	leaf->data.leaf.values[i] = value; // insert the value
	leaf->num_keys++;
}

int find_key_in_node(Node *node, int key) {
	if (!node) {
		return -1;
	}

	int r = node->num_keys - 1;
	int l = 0, m;

	while (l <= r) { // binary search in tree for the key
		m = (l + r) / 2;

		if (node->keys[m] == key) {
			return m;
		} else if (node->keys[m] > key) {
			r = m - 1;
		} else {
			l = m + 1;
		}
	}

	return l;
}

Node *insert_into_parent(Node *parent, Node *left_child, int key, Node *right_child) {
	if (!parent) { // no parent means the leaf was the root, create a brand new root
		Node *new_root = create_inner_node();

		new_root->keys[0] = key;
		new_root->data.inner.children[0] = left_child;
		new_root->data.inner.children[1] = right_child;
		new_root->num_keys = 1;

		return new_root;
	}

	if (parent->num_keys < MAX_KEYS) { // parent has space, just insert the guidepost
		int key_spot = find_key_in_node(parent, key); // finding where the new key fits

		for (int i = parent->num_keys; i > key_spot; i --) { // shifting keys and children right
			parent->data.inner.children[i + 1] = parent->data.inner.children[i];
			parent->keys[i] = parent->keys[i - 1];
		}

		parent->keys[key_spot] = key;
		parent->data.inner.children[key_spot + 1] = right_child;
		right_child->parent = parent; // keeping the parent pointer up to date

		parent->num_keys ++;
	} else {
		int split_boundary = parent->keys[parent->num_keys / 2]; // save the guidepost before it goes up

		Node *new_inner = split_inner_node(parent); // split the full parent, pushing guidepost to grandparent

		if (key < split_boundary) { // new key goes into the left (original) half
			insert_into_parent(parent, left_child, key, right_child);
		} else { // new key goes into the right (new) half
			insert_into_parent(new_inner, left_child, key, right_child);
		}
	}

	return parent;
}

void *find_child_index(Node *inner, int key) {
	if (!inner || inner->is_leaf) {
		return NULL;
	}

	int idx = find_key_in_node(inner, key);

	return inner->data.inner.children[idx];
}

Node *remove_from_leaf(Node *leaf, int key) {
	int k = find_key_in_node(leaf, key); // finding the index of the key to remove

	if (k >= leaf->num_keys || leaf->keys[k] != key) { // key doesn't exist, nothing to remove
		return leaf;
	}

	free(leaf->data.leaf.values[k]); // freeing the value before shifting over it

	for (int i = k; i < leaf->num_keys - 1; i ++) { // shifting keys and values left
		leaf->keys[i] = leaf->keys[i + 1];
		leaf->data.leaf.values[i] = leaf->data.leaf.values[i + 1];
	}

	leaf->num_keys --;
	leaf->keys[leaf->num_keys] = 0; // clearing the stale slot left after the shift
	leaf->data.leaf.values[leaf->num_keys] = NULL;

	return leaf;
}

Node *split_leaf(Node *leaf) {
	Node *new_leaf = create_leaf_node(); // creating the node where the keys and values will pe placed

	int middle = leaf->num_keys / 2; // finding the spliting point

	for (int i = middle; i < leaf->num_keys; i ++) { //shifting the keys and values
		new_leaf->keys[i - middle] = leaf->keys[i];
		new_leaf->data.leaf.values[i - middle] = leaf->data.leaf.values[i];
		leaf->data.leaf.values[i] = NULL;
		leaf->keys[i] = 0;

		new_leaf->num_keys++;
	}

	leaf->num_keys -= new_leaf->num_keys;

	new_leaf->data.leaf.next = leaf->data.leaf.next;
	leaf->data.leaf.next = new_leaf;

	return new_leaf;
}


Node *split_inner_node(Node *node) {
	if (!node) {
		return NULL;
	}

	int middle = node->num_keys / 2; // find the middle key
	int guidepost = node->keys[middle]; // save it before clearing — it goes UP to the parent

	Node *new_node = create_inner_node();

	// we need to delete this key from the node, we are pushing it to the parent
	node->keys[middle] = 0;
	// children[middle] stays in the left node — only the key goes up, not its child

	for (int i = middle + 1; i < node->num_keys; i ++) { // we are spliting the children evenly
		new_node->data.inner.children[i - middle - 1] = node->data.inner.children[i];
		node->data.inner.children[i] = NULL;
		new_node->keys[i - middle - 1] = node->keys[i];
		node->keys[i] = 0;
		new_node->num_keys ++;
	}

	new_node->data.inner.children[node->num_keys - 1 - middle] = node->data.inner.children[node->num_keys];
	node->data.inner.children[node->num_keys] = NULL; // clear the last child from the left node

	node->num_keys = middle; // left node keeps keys[0..middle-1] and children[0..middle]

	insert_into_parent(node->parent, node, guidepost, new_node); // push guidepost up to the parent

	return new_node;
}

bool try_borrow_from_left_sibling(Node *node, Node *parent, int index) {
	if (index <= 0) { //we don t have left siblings on negative positions
		return false;
	} 

	Node *sibling = parent->data.inner.children[index - 1]; // the left sibling

	if (sibling->num_keys > MAX_KEYS / 2) { //checking for balance
		if (node->is_leaf) { //if is leaf
			for (int i = node->num_keys; i > 0; i --) { // shifting the keys and values
				node->keys[i] = node->keys[i - 1];
				node->data.leaf.values[i] = node->data.leaf.values[i - 1];
			}

			node->keys[0] = sibling->keys[sibling->num_keys - 1]; // adding the borrowed key
			node->data.leaf.values[0] = sibling->data.leaf.values[sibling->num_keys]; // adding the borrowed value

			parent->keys[index - 1] = node->keys[0]; // adding the new key to the parent
		} else { // if is inners
			node->data.inner.children[node->num_keys + 1] = node->data.inner.children[node->num_keys];

			for (int i = node->num_keys; i > 0; i --) {// shifting the keys and children
				node->keys[i] = node->keys[i - 1];
				node->data.inner.children[i] = node->data.inner.children[i - 1];
			}

			node->keys[0] = parent->keys[index - 1]; // adding the borrowed key
			parent->keys[index - 1] = sibling->keys[sibling->num_keys - 1]; // adding the new key to the parent
			node->data.inner.children[0] = sibling->data.inner.children[sibling->num_keys - 1]; // adding the borrowed child
		}

		// adjusting the numbers of the keys
		sibling->num_keys--;
        node->num_keys++;

		return true;
	} else {
		return false;
	}
}

bool try_borrow_from_right_sibling(Node *node, Node *parent, int index) {
	if (index >= parent->num_keys) { // the index can not be greater than the maximum number admited
		return false;
	} 

	Node *sibling = parent->data.inner.children[index + 1];

	if (sibling->num_keys > MAX_KEYS / 2) { // checkinh for balance
		if (node->is_leaf) { //if is leaf
			node->keys[node->num_keys] = sibling->keys[0]; // adding the borrowed key
			node->data.leaf.values[node->num_keys] = sibling->data.leaf.values[0]; // adding the borrowed value


			for (int i = 0; i < sibling->num_keys - 1; i ++) { // shifting the keys and values
				sibling->keys[i] = sibling->keys[i + 1];
                sibling->data.leaf.values[i] = sibling->data.leaf.values[i + 1];
			}

			parent->keys[index] = sibling->keys[0]; // adding the new key to the parent
		} else { // if is inners
			node->keys[node->num_keys] = parent->keys[index]; // adding the borrowed key
			parent->keys[index] = sibling->keys[0]; // adding the new key to the parent
			node->data.inner.children[node->num_keys + 1] = sibling->data.inner.children[0]; // adding the borrowed child

			for (int i = 0; i < sibling->num_keys - 1; i ++) {
				sibling->keys[i] = sibling->keys[i + 1];
			}
			
			for (int i = 0; i < sibling->num_keys; i ++) {
				sibling->data.inner.children[i] = sibling->data.inner.children[i + 1];
			}
		}

		// adjusting the numbers of the keys
		sibling->num_keys--;
        node->num_keys++;

		return true;
	} else {
		return false;
	}
}