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
	// empty tree, create the first leaf
	if (!root) {
		root = create_leaf_node();
		insert_into_leaf_sorted(root, key, value);
		return root;
	}

	// traverse down to the correct leaf
	Node *leaf = root;
	while (!leaf->is_leaf) {
		int i = 0;
		while (i < leaf->num_keys && key > leaf->keys[i]) { // finding the correct child
			i++;
		}
		leaf = leaf->data.inner.children[i];
	}

	// leaf has space, just insert
	if (leaf->num_keys < MAX_KEYS) {
		insert_into_leaf_sorted(leaf, key, value);
		return root;
	}

	// leaf is full, split first then insert into the correct half
	Node *new_leaf = split_leaf(leaf);
	int guidepost = new_leaf->keys[0]; // smallest key of the right leaf goes up

	if (key >= guidepost) { // new key belongs in the right (new) leaf
		insert_into_leaf_sorted(new_leaf, key, value);
	} else { // new key belongs in the left (original) leaf
		insert_into_leaf_sorted(leaf, key, value);
	}

	// push guidepost key up into the parent
	Node *new_root = insert_into_parent(leaf->parent, leaf, guidepost, new_leaf);

	if (new_root) { // a new root was created (leaf was root, or cascading inner split)
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
	Node *leaf = root; // traverse down to the correct leaf
	while (leaf && !leaf->is_leaf) {
		int i = 0;
		while (i < leaf->num_keys && key >= leaf->keys[i]) {
			i++;
		}
		leaf = leaf->data.inner.children[i];
	}

	if (!leaf) return root; // empty tree

	int found = -1; // verify the key actually exists in the leaf
	for (int i = 0; i < leaf->num_keys; i++) {
		if (leaf->keys[i] == key) { found = i; break; }
	}
	if (found == -1) return root; // key not in tree

	remove_from_leaf(leaf, key); // remove from leaf (shifts remaining entries left, clears stale slot)

	Node *node = leaf;
	while (node && node->parent) {
		int min_keys = (MAX_KEYS + 1) / 2; // check for underflow — minimum occupancy is ceil(MAX_KEYS / 2)
		if (node->num_keys >= min_keys) {
			break; // no underflow, or node has enough keys
		}

		Node *parent = node->parent; // find which index this node sits at in its parent's children array
		int index = 0;
		while (index <= parent->num_keys && parent->data.inner.children[index] != node) {
			index++;
		}

		if (try_borrow_from_left_sibling(node, parent, index)) { // try to borrow from a sibling
			break; // borrow from left succeeded
		} else if (try_borrow_from_right_sibling(node, parent, index)) {
			break; // borrow from right succeeded
		} else { // borrow failed, we must merge
			if (index > 0) { // merge with left sibling
				Node *sibling = parent->data.inner.children[index - 1];
				merge_with_sibling(sibling, node, parent, index - 1);
			} else { // merge with right sibling
				Node *sibling = parent->data.inner.children[index + 1];
				merge_with_sibling(node, sibling, parent, index);
			}
			node = parent; // handle merge cascading up
		}
	}

	// handle root shrinking — if root ends up with 0 keys (only 1 child) after a merge
	if (!root->is_leaf && root->num_keys == 0) {
		Node *new_root = root->data.inner.children[0];
		new_root->parent = NULL;
		free(root);
		return new_root;
	}

	return root;
}

void freeNode(Node *node) {
	if (!node) { // checking if we have something to free
		return;
	}

	if (node->is_leaf) { // free leaf values first to avoid memory leaks
		for (int i = 0; i < node->num_keys; i++) {
			free(node->data.leaf.values[i]);
			node->data.leaf.values[i] = NULL;
		}
	} else { // if is inner, recursively free all children first
		for (int x = 0; x <= node->num_keys; x++) {
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
	if (!parent) { // no parent means the node was the root — create a brand new root
		Node *new_root = create_inner_node();

		new_root->keys[0] = key;
		new_root->data.inner.children[0] = left_child;
		new_root->data.inner.children[1] = right_child;
		new_root->num_keys = 1;
		left_child->parent = new_root; // keep parent pointers up to date
		right_child->parent = new_root;

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
		return NULL; // no new root was created
	} else {
		bool parent_was_root = (parent->parent == NULL);
		int split_boundary = parent->keys[parent->num_keys / 2]; // save the guidepost before it goes up

		Node *new_inner = split_inner_node(parent); // split the full parent, pushing guidepost to grandparent

		// if parent was the root, insert_into_parent (inside split_inner_node) just created a new root
		// and set parent->parent to point at it
		Node *new_root = parent_was_root ? parent->parent : NULL;

		if (key < split_boundary) { // new key goes into the left (original) half
			insert_into_parent(parent, left_child, key, right_child);
		} else { // new key goes into the right (new) half
			insert_into_parent(new_inner, left_child, key, right_child);
		}

		return new_root; // propagate new root upward (NULL if none was created at this level)
	}
}

Node *find_child_index(Node *inner, int key) {
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

	// we need to delete this key from the node, we are pushing it to the grandparent
	node->keys[middle] = 0;
	// children[middle] stays in the left node — only the key goes up, not its child

	for (int i = middle + 1; i < node->num_keys; i ++) { // we are splitting the children evenly
		new_node->data.inner.children[i - middle - 1] = node->data.inner.children[i];
		node->data.inner.children[i] = NULL;
		new_node->keys[i - middle - 1] = node->keys[i];
		node->keys[i] = 0;
		new_node->num_keys ++;
	}

	new_node->data.inner.children[node->num_keys - 1 - middle] = node->data.inner.children[node->num_keys];
	node->data.inner.children[node->num_keys] = NULL; // clear the last child from the left node

	node->num_keys = middle; // left node keeps keys[0..middle-1] and children[0..middle]

	// update parent pointers for all children that moved to new_node
	for (int i = 0; i <= new_node->num_keys; i++) {
		if (new_node->data.inner.children[i]) {
			new_node->data.inner.children[i]->parent = new_node;
		}
	}

	insert_into_parent(node->parent, node, guidepost, new_node); // push guidepost up; sets new_node->parent inside

	return new_node;
}

bool try_borrow_from_left_sibling(Node *node, Node *parent, int index) {
	if (index <= 0) { // no left sibling on negative positions
		return false;
	}

	Node *sibling = parent->data.inner.children[index - 1]; // the left sibling

	if (sibling->num_keys > MAX_KEYS / 2) { // sibling has spare keys
		if (node->is_leaf) {
			// shift node's keys and values right to make room at slot 0
			for (int i = node->num_keys; i > 0; i--) {
				node->keys[i] = node->keys[i - 1];
				node->data.leaf.values[i] = node->data.leaf.values[i - 1];
			}

			// borrow the last key and value from the left sibling
			node->keys[0] = sibling->keys[sibling->num_keys - 1];
			node->data.leaf.values[0] = sibling->data.leaf.values[sibling->num_keys - 1]; // fixed: was [num_keys]

			// clear the now-stale last slot in the sibling
			sibling->keys[sibling->num_keys - 1] = 0;
			sibling->data.leaf.values[sibling->num_keys - 1] = NULL;

			// update the guidepost in the parent (new smallest key of this node)
			parent->keys[index - 1] = node->keys[0];
		} else {
			// shift node's children right to make room at slot 0
			node->data.inner.children[node->num_keys + 1] = node->data.inner.children[node->num_keys];

			for (int i = node->num_keys; i > 0; i--) { // shifting the keys and children
				node->keys[i] = node->keys[i - 1];
				node->data.inner.children[i] = node->data.inner.children[i - 1];
			}

			node->keys[0] = parent->keys[index - 1]; // pull the separator from the parent down into node
			// bring sibling's rightmost child over as node's new leftmost child
			node->data.inner.children[0] = sibling->data.inner.children[sibling->num_keys]; // fixed: was [num_keys-1]
			if (node->data.inner.children[0]) {
				node->data.inner.children[0]->parent = node; // update parent pointer of adopted child
			}

			// push sibling's last key up to replace the parent separator
			parent->keys[index - 1] = sibling->keys[sibling->num_keys - 1];

			// clear the now-stale last key and child in the sibling
			sibling->keys[sibling->num_keys - 1] = 0;
			sibling->data.inner.children[sibling->num_keys] = NULL;
		}

		// adjust key counts
		sibling->num_keys--;
		node->num_keys++;

		return true;
	} else {
		return false;
	}
}

bool try_borrow_from_right_sibling(Node *node, Node *parent, int index) {
	if (index >= parent->num_keys) { // no right sibling available
		return false;
	}

	Node *sibling = parent->data.inner.children[index + 1];

	if (sibling->num_keys > MAX_KEYS / 2) { // sibling has spare keys
		if (node->is_leaf) {
			// borrow sibling's first key and value
			node->keys[node->num_keys] = sibling->keys[0];
			node->data.leaf.values[node->num_keys] = sibling->data.leaf.values[0];

			// shift sibling's keys and values left
			for (int i = 0; i < sibling->num_keys - 1; i++) {
				sibling->keys[i] = sibling->keys[i + 1];
				sibling->data.leaf.values[i] = sibling->data.leaf.values[i + 1];
			}

			// clear the now-stale last slot in the sibling
			sibling->keys[sibling->num_keys - 1] = 0;
			sibling->data.leaf.values[sibling->num_keys - 1] = NULL;

			// update the guidepost in the parent (new smallest key of sibling)
			parent->keys[index] = sibling->keys[0];
		} else {
			// pull the parent separator down into node's last key slot
			node->keys[node->num_keys] = parent->keys[index];
			// bring sibling's first child over as node's new last child
			node->data.inner.children[node->num_keys + 1] = sibling->data.inner.children[0];
			if (node->data.inner.children[node->num_keys + 1]) {
				node->data.inner.children[node->num_keys + 1]->parent = node; // update adopted child's parent
			}

			// push sibling's first key up to replace the parent separator
			parent->keys[index] = sibling->keys[0];

			// shift sibling's keys left and clear stale last key
			for (int i = 0; i < sibling->num_keys - 1; i++) {
				sibling->keys[i] = sibling->keys[i + 1];
			}
			sibling->keys[sibling->num_keys - 1] = 0;

			// shift sibling's children left and clear stale last child
			for (int i = 0; i < sibling->num_keys; i++) {
				sibling->data.inner.children[i] = sibling->data.inner.children[i + 1];
			}
			sibling->data.inner.children[sibling->num_keys] = NULL;
		}

		// adjust key counts
		sibling->num_keys--;
		node->num_keys++;

		return true;
	} else {
		return false;
	}
} 

Node *merge_with_sibling(Node *node, Node *sibling, Node *parent, int index) {
	if (node->num_keys + sibling->num_keys >= MAX_KEYS) { // checking if the merge is right
		return NULL;
	} 

	if (node->is_leaf) {
		for (int i = 0; i < sibling->num_keys; i ++) { // we are copying the keys and values 
			node->keys[node->num_keys + i] = sibling->keys[i];
			node->data.leaf.values[node->num_keys + i] = sibling->data.leaf.values[i];
		}

		node->data.leaf.next = sibling->data.leaf.next; // modifying the neighbour
		node->num_keys += sibling->num_keys; 
	} else {
		node->keys[node->num_keys] = parent->keys[index]; // drop the spliting key
		parent->keys[index] = 0;

		for (int i = 0; i < sibling->num_keys; i ++) { // we are copying the keys and children
			node->keys[node->num_keys + i + 1] = sibling->keys[i];
			node->data.inner.children[node->num_keys + i + 1] = sibling->data.inner.children[i];

			node->data.inner.children[node->num_keys + i + 1]->parent = node; // modifying the parent
		}

		node->data.inner.children[node->num_keys + sibling->num_keys + 1] = sibling->data.inner.children[sibling->num_keys];
		node->data.inner.children[node->num_keys + sibling->num_keys + 1]->parent = node;
		node->num_keys += sibling->num_keys + 1;
	}

	remove_from_parent(parent, index);
	free(sibling);

	return parent;
}

Node *remove_from_parent(Node *parent, int index) {
	for (int i = index; i < parent->num_keys - 1; i++) { // shifting the keys
        parent->keys[i] = parent->keys[i + 1];
    }

	for (int i = index + 1; i < parent->num_keys; i++) { // shifting the children
        parent->data.inner.children[i] = parent->data.inner.children[i + 1];
    }

	parent->num_keys --;
	parent->keys[parent->num_keys] = 0; // clearing stale key slot
	parent->data.inner.children[parent->num_keys + 1] = NULL; // clearing stale child pointer

	return parent;
}

bool helper_validate_tree(Node *node, int current_depth, int *expected_leaf_depth) {
	if (node->num_keys > MAX_KEYS) { // over the maximum limit
		return false;
	}

	if (node->num_keys < (MAX_KEYS + 1) / 2 && node->parent) { // bellow the minimum limit and is not the root
		return false;
	}

	for (int i = 0; i < node->num_keys - 1; i ++) {
		if (node->keys[i] >= node->keys[i + 1]) { // the keys are not in strictly ascending order, wrong
			return false;
		}
	}

	if (node->is_leaf) {
		if (*expected_leaf_depth == -1) { // this is the first leaf we encounter, record its depth
			*expected_leaf_depth = current_depth;
		} else {
			if (current_depth != *expected_leaf_depth) { // tree is unbalanced, leaves are at different depths
				return false;
			}

			if (node->data.leaf.next) {
				if (node->keys[node->num_keys - 1] >= node->data.leaf.next->keys[0]) { // the sibling always has to have greater keys
					return false;
				}
			}
		}
	} else {
		for (int i = 0; i <= node->num_keys; i ++) {
			if (!node->data.inner.children[i]) { // should not be null
				return false;	
			}
		}

		if (node->data.inner.children[node->num_keys + 1]) { // should be null, verifying the stale slots are cleared
			return false;
		}

		for (int i = 0; i <= node->num_keys; i ++) { // recursively validate all children
			if (!helper_validate_tree(node->data.inner.children[i], current_depth + 1, expected_leaf_depth)) {
				return false;
			}
		}

		for (int i = 0; i < node->num_keys; i ++) {
			if (!key_check_less(node->data.inner.children[i], node->keys[i])) { // all keys in the left child's subtree must be strictly less
				return false;
			}
			if (!key_check_greater_eq(node->data.inner.children[i + 1], node->keys[i])) { // all keys in the right child's subtree must be greater or equal
				return false;
			}
		}
	}

	return true; // all invariants passed
}

bool key_check_less(Node *a, int n) {
	if (!a) { // base case for empty node
		return true;
	}

	for (int i = 0; i < a->num_keys; i ++) {
		if (a->keys[i] >= n) { // key violates the strict less-than boundary
			return false;
		}
	}

	if (!a->is_leaf) { // recursively check all descendant leaves
		for (int i = 0; i <= a->num_keys; i ++) {
			if (!key_check_less(a->data.inner.children[i], n)) {
				return false;
			}
		}
	}

	return true;
}

bool key_check_greater_eq(Node *a, int n) {
	if (!a) { // base case for empty node
		return true;
	}

	for (int i = 0; i < a->num_keys; i ++) {
		if (a->keys[i] < n) { // key violates the greater-than-or-equal boundary
			return false;
		}
	}

	if (!a->is_leaf) { // recursively check all descendant leaves
		for (int i = 0; i <= a->num_keys; i ++) {
			if (!key_check_greater_eq(a->data.inner.children[i], n)) {
				return false;
			}
		}
	}

	return true;
}

bool validate_tree(Node *root) {
	if (!root) {
		return true;
	}

	int leaf_depth = -1;
	return helper_validate_tree(root, 0, &leaf_depth);
}