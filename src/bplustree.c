#include <stdio.h>
#include <stdlib.h>
#include "bplustree.h"

#include "serialize.h"

page_id_t allocate_node_page(BufferPool *bp) {
    if (!bp || !bp->pm) return INVALID_PAGE_ID;
    return pm_allocate_page(bp->pm);
}

Tree *createTree() {
	Tree *newTree = malloc(sizeof(Tree));

	if (newTree) {
		newTree->root_id = INVALID_PAGE_ID;
	}

	return newTree;
}

page_id_t create_leaf_node(BufferPool *bp) {
	page_id_t id = allocate_node_page(bp);
	if (id == INVALID_PAGE_ID) return INVALID_PAGE_ID;

	void *raw = bp_fetch_page(bp, id);
	if (!raw) return INVALID_PAGE_ID;

	Node *new_node = malloc(sizeof(Node));
	if (!new_node) {
		bp_unpin(bp, id, false);
		return INVALID_PAGE_ID;
	}
	new_node->page_id = id;
	new_node->is_leaf = true;
	new_node->num_keys = 0;
	new_node->parent_id = INVALID_PAGE_ID; // no parent yet

	for (int i = 0; i < MAX_KEYS; i++) {
		new_node->data.leaf.values[i] = (int64_t)(0);
	}
	new_node->data.leaf.next_id = INVALID_PAGE_ID;

	serialize_node(new_node, raw);
	bp_unpin(bp, id, true);
	free(new_node);

	return id;
}

page_id_t create_inner_node(BufferPool *bp) {
	page_id_t id = allocate_node_page(bp);
	if (id == INVALID_PAGE_ID) return INVALID_PAGE_ID;

	void *raw = bp_fetch_page(bp, id);
	if (!raw) return INVALID_PAGE_ID;

	Node *new_node = malloc(sizeof(Node));
	if (!new_node) {
		bp_unpin(bp, id, false);
		return INVALID_PAGE_ID;
	}
	new_node->page_id = id;
	new_node->is_leaf = false;
	new_node->num_keys = 0;
	new_node->parent_id = INVALID_PAGE_ID; // no parent yet

	for (int i = 0; i < MAX_KEYS + 1; i++) {
		new_node->data.inner.children[i] = INVALID_PAGE_ID;
	}

	serialize_node(new_node, raw);
	bp_unpin(bp, id, true);
	free(new_node);

	return id;
}




void insert_into_leaf_sorted(Node *leaf, int key, void *value) {
	int i = 0;
	while (i < leaf->num_keys && key > leaf->keys[i]) i++;
	if (i < leaf->num_keys && leaf->keys[i] == key) return;
	for (int j = leaf->num_keys; j > i; j--) {
		leaf->keys[j] = leaf->keys[j - 1];
		leaf->data.leaf.values[j] = leaf->data.leaf.values[j - 1];
	}
	leaf->keys[i] = key;
	leaf->data.leaf.values[i] = (int64_t)(value);
	leaf->num_keys++;
}

Node *split_leaf(BufferPool *bp, Node *leaf, void **new_raw_out) {
	page_id_t new_id = create_leaf_node(bp);
	void *raw = bp_fetch_page(bp, new_id);
	Node *new_leaf = deserialize_node(raw);
	
	int middle = leaf->num_keys / 2;
	for (int i = middle; i < leaf->num_keys; i ++) {
		new_leaf->keys[i - middle] = leaf->keys[i];
		new_leaf->data.leaf.values[i - middle] = leaf->data.leaf.values[i];
		leaf->data.leaf.values[i] = 0;
		leaf->keys[i] = 0;
		new_leaf->num_keys++;
	}
	leaf->num_keys -= new_leaf->num_keys;
	new_leaf->data.leaf.next_id = leaf->data.leaf.next_id;
	leaf->data.leaf.next_id = new_leaf->page_id;
	
	*new_raw_out = raw;
	return new_leaf;
}

Node *split_inner_node(BufferPool *bp, Node *node, void **new_raw_out, int *middle_key_out) {
	page_id_t new_id = create_inner_node(bp);
	void *raw = bp_fetch_page(bp, new_id);
	Node *new_node = deserialize_node(raw);

	int middle = node->num_keys / 2;
	/* Save the key that will be pushed up to the parent BEFORE clearing it. */
	int pushed_key = node->keys[middle];

	for (int i = middle + 1; i < node->num_keys; i ++) {
		new_node->keys[i - middle - 1] = node->keys[i];
		new_node->data.inner.children[i - middle - 1] = node->data.inner.children[i];
		node->keys[i] = 0;
		node->data.inner.children[i] = INVALID_PAGE_ID;
		new_node->num_keys++;
	}
	new_node->data.inner.children[node->num_keys - 1 - middle] = node->data.inner.children[node->num_keys];
	node->data.inner.children[node->num_keys] = INVALID_PAGE_ID;
	node->keys[middle] = 0; /* clear the pushed-up key slot from the left node */
	node->num_keys -= (new_node->num_keys + 1);

	for (int i = 0; i <= new_node->num_keys; i++) {
        page_id_t child_id = new_node->data.inner.children[i];
		if (child_id != INVALID_PAGE_ID) {
            void *c_raw = bp_fetch_page(bp, child_id);
            if (!c_raw) continue;
            Node *c_node = deserialize_node(c_raw);
            if (!c_node) { bp_unpin(bp, child_id, false); continue; }
			c_node->parent_id = new_node->page_id;
            serialize_node(c_node, c_raw);
            bp_unpin(bp, child_id, true);
            free(c_node);
		}
	}

	*new_raw_out = raw;
	*middle_key_out = pushed_key;
	return new_node;
}

void helper_insert_into_parent_node(Node *parent, page_id_t right_child, int key) {
    int key_spot = find_key_in_node(parent, key);
    for (int i = parent->num_keys; i > key_spot; i --) {
        parent->data.inner.children[i + 1] = parent->data.inner.children[i];
        parent->keys[i] = parent->keys[i - 1];
    }
    parent->keys[key_spot] = key;
    parent->data.inner.children[key_spot + 1] = right_child;
    parent->num_keys ++;
}

page_id_t insert_into_parent(BufferPool *bp, page_id_t parent_id, page_id_t left_child, int key, page_id_t right_child) {
	if (parent_id == INVALID_PAGE_ID) {
		page_id_t new_root_id = create_inner_node(bp);
		void *raw = bp_fetch_page(bp, new_root_id);
		Node *new_root = deserialize_node(raw);

		new_root->keys[0] = key;
		new_root->data.inner.children[0] = left_child;
		new_root->data.inner.children[1] = right_child;
		new_root->num_keys = 1;

		if (left_child != INVALID_PAGE_ID) {
            void *l_raw = bp_fetch_page(bp, left_child);
            Node *l_node = deserialize_node(l_raw);
			l_node->parent_id = new_root_id;
            serialize_node(l_node, l_raw);
            bp_unpin(bp, left_child, true);
            free(l_node);
		}
		if (right_child != INVALID_PAGE_ID) {
            void *r_raw = bp_fetch_page(bp, right_child);
            Node *r_node = deserialize_node(r_raw);
			r_node->parent_id = new_root_id;
            serialize_node(r_node, r_raw);
            bp_unpin(bp, right_child, true);
            free(r_node);
		}

        serialize_node(new_root, raw);
        bp_unpin(bp, new_root_id, true);
        free(new_root);
		return new_root_id;
	}

    void *raw = bp_fetch_page(bp, parent_id);
    Node *parent = deserialize_node(raw);

	if (parent->num_keys < MAX_KEYS) {
        helper_insert_into_parent_node(parent, right_child, key);
		
		if (right_child != INVALID_PAGE_ID) {
            void *r_raw = bp_fetch_page(bp, right_child);
            Node *r_node = deserialize_node(r_raw);
			r_node->parent_id = parent_id;
            serialize_node(r_node, r_raw);
            bp_unpin(bp, right_child, true);
            free(r_node);
		}

        serialize_node(parent, raw);
        bp_unpin(bp, parent_id, true);
        free(parent);
		return INVALID_PAGE_ID; 
	} else {
		int split_boundary;
        void *new_inner_raw;
		Node *new_inner = split_inner_node(bp, parent, &new_inner_raw, &split_boundary);
        page_id_t new_inner_id = new_inner->page_id;

		if (key < split_boundary) {
            helper_insert_into_parent_node(parent, right_child, key);
            if (right_child != INVALID_PAGE_ID) {
                void *r_raw = bp_fetch_page(bp, right_child);
                Node *r_node = deserialize_node(r_raw);
                r_node->parent_id = parent_id;
                serialize_node(r_node, r_raw);
                bp_unpin(bp, right_child, true);
                free(r_node);
            }
		} else {
            helper_insert_into_parent_node(new_inner, right_child, key);
            if (right_child != INVALID_PAGE_ID) {
                void *r_raw = bp_fetch_page(bp, right_child);
                Node *r_node = deserialize_node(r_raw);
                r_node->parent_id = new_inner_id;
                serialize_node(r_node, r_raw);
                bp_unpin(bp, right_child, true);
                free(r_node);
            }
		}

        page_id_t parent_parent_id = parent->parent_id;

        serialize_node(parent, raw);
        bp_unpin(bp, parent_id, true);
        free(parent);

        serialize_node(new_inner, new_inner_raw);
        bp_unpin(bp, new_inner_id, true);
        free(new_inner);

        page_id_t res = insert_into_parent(bp, parent_parent_id, parent_id, split_boundary, new_inner_id);
        
        if (res != INVALID_PAGE_ID) {
            return res; 
        }

		page_id_t curr = parent_id;
		while (curr != INVALID_PAGE_ID) {
            void *c_raw = bp_fetch_page(bp, curr);
            Node *c_node = deserialize_node(c_raw);
            page_id_t p = c_node->parent_id;
            bp_unpin(bp, curr, false);
            free(c_node);
            if (p == INVALID_PAGE_ID) return curr;
            curr = p;
		}
		return curr;
	}
}

page_id_t insert(BufferPool *bp, page_id_t root_id, int key, void *value) {
	if (root_id == INVALID_PAGE_ID) {
		root_id = create_leaf_node(bp);
		void *raw = bp_fetch_page(bp, root_id);
		Node *root = deserialize_node(raw);
		insert_into_leaf_sorted(root, key, value);
		serialize_node(root, raw);
		bp_unpin(bp, root_id, true);
		free(root);
		return root_id;
	}

	page_id_t curr_id = root_id;
	void *raw = bp_fetch_page(bp, curr_id);
	/* 'curr' traverses inner nodes until it reaches the target leaf.
	 * After the while loop it is guaranteed to be a leaf node. */
	Node *curr = deserialize_node(raw);

	while (!curr->is_leaf) {
		int i = 0;
		while (i < curr->num_keys && key >= curr->keys[i]) i++;
		page_id_t next_id = curr->data.inner.children[i];
		
		bp_unpin(bp, curr_id, false);
		free(curr);

		curr_id = next_id;
		raw = bp_fetch_page(bp, curr_id);
		curr = deserialize_node(raw);
	}

	if (curr->num_keys < MAX_KEYS) {
		insert_into_leaf_sorted(curr, key, value);
		serialize_node(curr, raw);
		bp_unpin(bp, curr_id, true);
		free(curr);
		return root_id;
	}

	void *new_raw;
	Node *new_leaf = split_leaf(bp, curr, &new_raw);
    page_id_t new_leaf_id = new_leaf->page_id;
	int guidepost = new_leaf->keys[0];

	if (key >= guidepost) {
		insert_into_leaf_sorted(new_leaf, key, value);
	} else {
		insert_into_leaf_sorted(curr, key, value);
	}

    page_id_t parent_id = curr->parent_id;

	serialize_node(curr, raw);
	bp_unpin(bp, curr_id, true);
	free(curr);

    serialize_node(new_leaf, new_raw);
    bp_unpin(bp, new_leaf_id, true);
    free(new_leaf);

	page_id_t new_root = insert_into_parent(bp, parent_id, curr_id, guidepost, new_leaf_id);
	if (new_root != INVALID_PAGE_ID) {
		return new_root;
	}

	return root_id;
}

void *search(BufferPool *bp, page_id_t node_id, int key) {
	if (node_id == INVALID_PAGE_ID || !bp) {
		return NULL;
	}

	void *raw = bp_fetch_page(bp, node_id);
	if (!raw) return NULL;

	Node *node = deserialize_node(raw);
	if (!node) {
		bp_unpin(bp, node_id, false);
		return NULL;
	}

	void *result = NULL;

	if (node->is_leaf) { // if leaf, going through its keys to see if our target key is there
		for (int i = 0; i < node->num_keys; i ++) {
			if (node->keys[i] == key) { // found the key, return the value
				result = (void *)node->data.leaf.values[i];
				break;
			}
		}
		bp_unpin(bp, node_id, false);
		free(node);
		return result;
	} else { // if inner, checking the children which keys are smaller than our key
		int child_index = node->num_keys;
		for (int i = 0; i < node->num_keys; i ++) {
			if (key < node->keys[i]) {
				child_index = i;
				break;
			}
		}
		page_id_t next_child = node->data.inner.children[child_index];
		bp_unpin(bp, node_id, false);
		free(node);
		return search(bp, next_child, key);
	}
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

Node *remove_from_leaf(Node *leaf, int key) {
	int k = find_key_in_node(leaf, key); // finding the index of the key to remove

	if (k >= leaf->num_keys || leaf->keys[k] != key) { // key doesn't exist, nothing to remove
		return leaf;
	}

	 // freeing the value before shifting over it

	for (int i = k; i < leaf->num_keys - 1; i ++) { // shifting keys and values left
		leaf->keys[i] = leaf->keys[i + 1];
		leaf->data.leaf.values[i] = (int64_t)(leaf->data.leaf.values[i + 1]);
	}

	leaf->num_keys --;
	leaf->keys[leaf->num_keys] = 0; // clearing the stale slot left after the shift
	leaf->data.leaf.values[leaf->num_keys] = (int64_t)(NULL);

	return leaf;
}

bool range_search(BufferPool *bp, page_id_t node_id, int start, int end, int result_keys[], void *result_values[], int *num_res) {
	if (node_id == INVALID_PAGE_ID || !bp) {
		return false;
	}

	void *raw = bp_fetch_page(bp, node_id);
	if (!raw) return false;

	Node *node = deserialize_node(raw);
	if (!node) {
		bp_unpin(bp, node_id, false);
		return false;
	}

	bool result = false;
	
	if (node->is_leaf) { // if leaf, going through its keys to see where our range starts
		for (int i = 0; i < node->num_keys; i ++) {
			if (node->keys[i] >= start) { // found the start key or a bigger one
				for (int j = i; j < node->num_keys; j ++) { 
					if (node->keys[j] > end) {
						result = true;
						break;
					}

					result_keys[*num_res] = node->keys[j];
					result_values[*num_res] = node->data.leaf.values[j];
					(*num_res) ++; 
				}
				break; // going out to check the siblings
			}
		}

		if (result == true) {
			bp_unpin(bp, node_id, false);
			free(node);
			return true;
		}

		page_id_t curr_id = node->data.leaf.next_id;
		bp_unpin(bp, node_id, false);
		free(node);

		while (curr_id != INVALID_PAGE_ID) { // going right through the linked leaves
			raw = bp_fetch_page(bp, curr_id);
			if (!raw) return true;
			Node *curr_node = deserialize_node(raw);
			if (!curr_node) {
				bp_unpin(bp, curr_id, false);
				return true;
			}

			bool stop = false;
			for (int k = 0; k < curr_node->num_keys; k ++) {
				if (curr_node->keys[k] > end) { 
					stop = true;
					break;
				}

				if (curr_node->keys[k] >= start) { 
					result_keys[*num_res] = curr_node->keys[k];
					result_values[*num_res] = curr_node->data.leaf.values[k];
					(*num_res) ++;
				}
			}
			
			page_id_t next_id = curr_node->data.leaf.next_id;
			bp_unpin(bp, curr_id, false);
			free(curr_node);

			if (stop) return true;
			curr_id = next_id;
		}
		return true;

	} else { // if inner, checking the children which keys are smaller than our start key
		int child_index = node->num_keys;
		for (int i = 0; i < node->num_keys; i ++) {
			if (start < node->keys[i]) { 
				child_index = i;
				break;
			}
		}

		page_id_t next_child = node->data.inner.children[child_index];
		bp_unpin(bp, node_id, false);
		free(node);
		// recursive search
		return range_search(bp, next_child, start, end, result_keys, result_values, num_res); 
	}
}

page_id_t deleteNode(BufferPool *bp, page_id_t root_id, int key) {
	if (root_id == INVALID_PAGE_ID || !bp) return INVALID_PAGE_ID;

    page_id_t curr_id = root_id;
	void *raw = bp_fetch_page(bp, curr_id);
	/* 'curr' traverses inner nodes until it reaches the target leaf.
	 * After the while loop it is guaranteed to be a leaf node. */
	Node *curr = deserialize_node(raw);

	while (!curr->is_leaf) {
		int i = 0;
		while (i < curr->num_keys && key >= curr->keys[i]) i++;
		page_id_t next_id = curr->data.inner.children[i];

        bp_unpin(bp, curr_id, false);
        free(curr);
        
        curr_id = next_id;
        raw = bp_fetch_page(bp, curr_id);
        curr = deserialize_node(raw);
	}

	int found = -1;
	for (int i = 0; i < curr->num_keys; i++) {
		if (curr->keys[i] == key) { found = i; break; }
	}
	if (found == -1) {
        bp_unpin(bp, curr_id, false);
        free(curr);
        return root_id;
    }

	remove_from_leaf(curr, key);

	Node *node = curr;
    page_id_t node_id = curr_id;
    void *node_raw = raw;

	while (node && node->parent_id != INVALID_PAGE_ID) {
		int min_keys = node->is_leaf ? (MAX_KEYS + 1) / 2 : (MAX_KEYS + 1) / 2 - 1;
		if (node->num_keys >= min_keys) break; 

        page_id_t parent_id = node->parent_id;
        void *parent_raw = bp_fetch_page(bp, parent_id);
		Node *parent = deserialize_node(parent_raw);
        
		int index = 0;
		while (index <= parent->num_keys && parent->data.inner.children[index] != node_id) {
			index++;
		}

		if (try_borrow_from_left_sibling(bp, node, parent, index)) { 
            serialize_node(parent, parent_raw);
            bp_unpin(bp, parent_id, true);
            free(parent);
			break; 
		} else if (try_borrow_from_right_sibling(bp, node, parent, index)) {
            serialize_node(parent, parent_raw);
            bp_unpin(bp, parent_id, true);
            free(parent);
			break;
		} else {
			if (index > 0) {
                page_id_t sibling_id = parent->data.inner.children[index - 1];
                void *sibling_raw = bp_fetch_page(bp, sibling_id);
				Node *sibling = deserialize_node(sibling_raw);

				merge_with_sibling(bp, sibling, node, parent, index - 1);
                
                serialize_node(sibling, sibling_raw);
                bp_unpin(bp, sibling_id, true);
                free(sibling);
			} else {
                page_id_t sibling_id = parent->data.inner.children[index + 1];
                void *sibling_raw = bp_fetch_page(bp, sibling_id);
				Node *sibling = deserialize_node(sibling_raw);

				merge_with_sibling(bp, node, sibling, parent, index);
                
                serialize_node(sibling, sibling_raw);
                bp_unpin(bp, sibling_id, true);
                free(sibling);
			}

            serialize_node(node, node_raw);
            bp_unpin(bp, node_id, true);
            free(node);

			node = parent;
            node_id = parent_id;
            node_raw = parent_raw;
		}
	}

    /* If the last node from the rebalance loop IS the root, handle the
     * root-collapse check in-place to avoid an unpin + immediate re-fetch
     * of the exact same page (Issue #3 from the code review). */
    if (node && node_id == root_id) {
        if (!node->is_leaf && node->num_keys == 0) {
            page_id_t new_root_id = node->data.inner.children[0];
            bp_unpin(bp, root_id, false);
            free(node);

            void *new_root_raw = bp_fetch_page(bp, new_root_id);
            Node *new_root = deserialize_node(new_root_raw);
            new_root->parent_id = INVALID_PAGE_ID;
            serialize_node(new_root, new_root_raw);
            bp_unpin(bp, new_root_id, true);
            free(new_root);

            return new_root_id;
        }
        serialize_node(node, node_raw);
        bp_unpin(bp, node_id, true);
        free(node);
        return root_id;
    }

    if (node) {
        serialize_node(node, node_raw);
        bp_unpin(bp, node_id, true);
        free(node);
    }

    /* node_id != root_id: the root was not part of the rebalance loop,
     * but a merge at a lower level may have drained it to 0 keys. */
    void *root_raw = bp_fetch_page(bp, root_id);
    Node *r = deserialize_node(root_raw);
	if (!r->is_leaf && r->num_keys == 0) {
		page_id_t new_root_id = r->data.inner.children[0];
        bp_unpin(bp, root_id, false);
        free(r);

        void *new_root_raw = bp_fetch_page(bp, new_root_id);
		Node *new_root = deserialize_node(new_root_raw);
		new_root->parent_id = INVALID_PAGE_ID;
        serialize_node(new_root, new_root_raw);
        bp_unpin(bp, new_root_id, true);
        free(new_root);

		return new_root_id;
	}
    bp_unpin(bp, root_id, false);
    free(r);

	return root_id;
}

bool try_borrow_from_left_sibling(BufferPool *bp, Node *node, Node *parent, int index) {
	if (index <= 0) return false;

    page_id_t sibling_id = parent->data.inner.children[index - 1];
    void *raw = bp_fetch_page(bp, sibling_id);
	if (!raw) return false;
	Node *sibling = deserialize_node(raw);
	if (!sibling) { bp_unpin(bp, sibling_id, false); return false; } 

	if (sibling->num_keys > MAX_KEYS / 2) { 
		if (node->is_leaf) {
			for (int i = node->num_keys; i > 0; i--) {
				node->keys[i] = node->keys[i - 1];
				node->data.leaf.values[i] = node->data.leaf.values[i - 1];
			}
			node->keys[0] = sibling->keys[sibling->num_keys - 1];
			node->data.leaf.values[0] = sibling->data.leaf.values[sibling->num_keys - 1];
			sibling->keys[sibling->num_keys - 1] = 0;
			sibling->data.leaf.values[sibling->num_keys - 1] = 0;
			parent->keys[index - 1] = node->keys[0];
		} else {
			node->data.inner.children[node->num_keys + 1] = node->data.inner.children[node->num_keys];
			for (int i = node->num_keys; i > 0; i--) {
				node->keys[i] = node->keys[i - 1];
				node->data.inner.children[i] = node->data.inner.children[i - 1];
			}
			node->keys[0] = parent->keys[index - 1];
			node->data.inner.children[0] = sibling->data.inner.children[sibling->num_keys];
			
            page_id_t adopted_id = node->data.inner.children[0];
            if (adopted_id != INVALID_PAGE_ID) {
                void *a_raw = bp_fetch_page(bp, adopted_id);
                Node *a_node = deserialize_node(a_raw);
                a_node->parent_id = node->page_id;
                serialize_node(a_node, a_raw);
                bp_unpin(bp, adopted_id, true);
                free(a_node);
			}

			parent->keys[index - 1] = sibling->keys[sibling->num_keys - 1];
			sibling->keys[sibling->num_keys - 1] = 0;
			sibling->data.inner.children[sibling->num_keys] = INVALID_PAGE_ID;
		}

		sibling->num_keys--;
		node->num_keys++;
        
        serialize_node(sibling, raw);
        bp_unpin(bp, sibling_id, true);
        free(sibling);
		return true;
	} else {
        bp_unpin(bp, sibling_id, false);
        free(sibling);
		return false;
	}
}

bool try_borrow_from_right_sibling(BufferPool *bp, Node *node, Node *parent, int index) {
	if (index >= parent->num_keys) return false;

    page_id_t sibling_id = parent->data.inner.children[index + 1];
    void *raw = bp_fetch_page(bp, sibling_id);
	if (!raw) return false;
	Node *sibling = deserialize_node(raw);
	if (!sibling) { bp_unpin(bp, sibling_id, false); return false; }

	if (sibling->num_keys > MAX_KEYS / 2) { 
		if (node->is_leaf) {
			node->keys[node->num_keys] = sibling->keys[0];
			node->data.leaf.values[node->num_keys] = sibling->data.leaf.values[0];

			for (int i = 0; i < sibling->num_keys - 1; i++) {
				sibling->keys[i] = sibling->keys[i + 1];
				sibling->data.leaf.values[i] = sibling->data.leaf.values[i + 1];
			}
			sibling->keys[sibling->num_keys - 1] = 0;
			sibling->data.leaf.values[sibling->num_keys - 1] = 0;
			parent->keys[index] = sibling->keys[0];
		} else {
			node->keys[node->num_keys] = parent->keys[index];
			node->data.inner.children[node->num_keys + 1] = sibling->data.inner.children[0];
			
            page_id_t adopted_id = node->data.inner.children[node->num_keys + 1];
            if (adopted_id != INVALID_PAGE_ID) {
                void *a_raw = bp_fetch_page(bp, adopted_id);
                Node *a_node = deserialize_node(a_raw);
                a_node->parent_id = node->page_id;
                serialize_node(a_node, a_raw);
                bp_unpin(bp, adopted_id, true);
                free(a_node);
			}

			parent->keys[index] = sibling->keys[0];

			for (int i = 0; i < sibling->num_keys - 1; i++) {
				sibling->keys[i] = sibling->keys[i + 1];
			}
			sibling->keys[sibling->num_keys - 1] = 0;

			for (int i = 0; i < sibling->num_keys; i++) {
				sibling->data.inner.children[i] = sibling->data.inner.children[i + 1];
			}
			sibling->data.inner.children[sibling->num_keys] = INVALID_PAGE_ID;
		}

		sibling->num_keys--;
		node->num_keys++;

        serialize_node(sibling, raw);
        bp_unpin(bp, sibling_id, true);
        free(sibling);
		return true;
	} else {
        bp_unpin(bp, sibling_id, false);
        free(sibling);
		return false;
	}
} 

/* CONTRACT: merge_with_sibling modifies 'node', 'sibling', and 'parent' in
 * memory but serializes NONE of them.  The caller owns all three dirty
 * heap nodes and is responsible for:
 *   - serializing + unpinning 'sibling' (dirty=true) immediately after.
 *   - serializing + unpinning 'node'   (dirty=true) once rebalancing ends.
 *   - serializing + unpinning 'parent' (dirty=true) once rebalancing ends
 *     (parent's key is removed by remove_from_parent inside this function). */
Node *merge_with_sibling(BufferPool *bp, Node *node, Node *sibling, Node *parent, int index) {
	int merged = node->is_leaf ? node->num_keys + sibling->num_keys : node->num_keys + sibling->num_keys + 1;
	if (merged > MAX_KEYS) return NULL;

	if (node->is_leaf) {
		for (int i = 0; i < sibling->num_keys; i ++) { 
			node->keys[node->num_keys + i] = sibling->keys[i];
			node->data.leaf.values[node->num_keys + i] = sibling->data.leaf.values[i];
		}
		node->data.leaf.next_id = sibling->data.leaf.next_id;
		node->num_keys += sibling->num_keys; 
	} else {
		node->keys[node->num_keys] = parent->keys[index];
		parent->keys[index] = 0;

		for (int i = 0; i < sibling->num_keys; i ++) { 
			node->keys[node->num_keys + i + 1] = sibling->keys[i];
			node->data.inner.children[node->num_keys + i + 1] = sibling->data.inner.children[i];

            page_id_t child_id = sibling->data.inner.children[i];
            if (child_id != INVALID_PAGE_ID) {
                void *c_raw = bp_fetch_page(bp, child_id);
                if (!c_raw) continue;
                Node *c_node = deserialize_node(c_raw);
                if (!c_node) { bp_unpin(bp, child_id, false); continue; }
                c_node->parent_id = node->page_id;
                serialize_node(c_node, c_raw);
                bp_unpin(bp, child_id, true);
                free(c_node);
            }
		}

		node->data.inner.children[node->num_keys + sibling->num_keys + 1] = sibling->data.inner.children[sibling->num_keys];
        page_id_t last_child_id = sibling->data.inner.children[sibling->num_keys];
        if (last_child_id != INVALID_PAGE_ID) {
            void *c_raw = bp_fetch_page(bp, last_child_id);
            if (c_raw) {
                Node *c_node = deserialize_node(c_raw);
                if (c_node) {
                    c_node->parent_id = node->page_id;
                    serialize_node(c_node, c_raw);
                    bp_unpin(bp, last_child_id, true);
                    free(c_node);
                } else {
                    bp_unpin(bp, last_child_id, false);
                }
            }
        }
		node->num_keys += sibling->num_keys + 1;
	}

	remove_from_parent(bp, parent, index);
	return parent;
}

Node *remove_from_parent(BufferPool *bp, Node *parent, int index) {
	for (int i = index; i < parent->num_keys - 1; i++) {
        parent->keys[i] = parent->keys[i + 1];
    }
	for (int i = index + 1; i < parent->num_keys; i++) {
        parent->data.inner.children[i] = parent->data.inner.children[i + 1];
    }
	parent->num_keys --;
	parent->keys[parent->num_keys] = 0; 
	parent->data.inner.children[parent->num_keys + 1] = INVALID_PAGE_ID; 
	return parent;
}

bool helper_validate_tree(BufferPool *bp, page_id_t node_id, int current_depth, int *expected_leaf_depth) {
    if (node_id == INVALID_PAGE_ID) return true;
    
    void *raw = bp_fetch_page(bp, node_id);
    if (!raw) return false;
    Node *node = deserialize_node(raw);
    if (!node) return false;
    
	if (node->num_keys > MAX_KEYS) {
        bp_unpin(bp, node_id, false);
        free(node);
		return false;
	}

	int min_keys = node->is_leaf ? (MAX_KEYS + 1) / 2 : (MAX_KEYS + 1) / 2 - 1;

	if (node->num_keys < min_keys && node->parent_id != INVALID_PAGE_ID) {
        bp_unpin(bp, node_id, false);
        free(node);
		return false;
	}

	for (int i = 0; i < node->num_keys - 1; i ++) {
		if (node->keys[i] >= node->keys[i + 1]) {
            bp_unpin(bp, node_id, false);
            free(node);
			return false;
		}
	}

	if (node->is_leaf) {
		if (*expected_leaf_depth == -1) { 
			*expected_leaf_depth = current_depth;
		} else {
			if (current_depth != *expected_leaf_depth) {
                bp_unpin(bp, node_id, false);
                free(node);
				return false;
			}
            
            page_id_t next_id = node->data.leaf.next_id;
			if (next_id != INVALID_PAGE_ID) {
                void *n_raw = bp_fetch_page(bp, next_id);
                Node *next_node = deserialize_node(n_raw);
				if (node->keys[node->num_keys - 1] >= next_node->keys[0]) {
                    bp_unpin(bp, next_id, false);
                    free(next_node);
                    bp_unpin(bp, node_id, false);
                    free(node);
					return false;
				}
                bp_unpin(bp, next_id, false);
                free(next_node);
			}
		}
        bp_unpin(bp, node_id, false);
        free(node);
        return true;
	} else {
		for (int i = 0; i <= node->num_keys; i ++) {
			if (node->data.inner.children[i] == INVALID_PAGE_ID) {
                bp_unpin(bp, node_id, false);
                free(node);
				return false;	
			}
		}

		if (node->num_keys < MAX_KEYS && node->data.inner.children[node->num_keys + 1] != INVALID_PAGE_ID) {
            bp_unpin(bp, node_id, false);
            free(node);
			return false;
		}

		page_id_t children[MAX_KEYS + 1];
		int keys[MAX_KEYS];
		int num_keys = node->num_keys;
		for (int i = 0; i <= num_keys; i++) {
			children[i] = node->data.inner.children[i];
		}
		for (int i = 0; i < num_keys; i++) {
			keys[i] = node->keys[i];
		}

		bp_unpin(bp, node_id, false);
		free(node);

		for (int i = 0; i <= num_keys; i ++) {
			if (!helper_validate_tree(bp, children[i], current_depth + 1, expected_leaf_depth)) {
				return false;
			}
		}

		for (int i = 0; i < num_keys; i ++) {
			if (!key_check_less(bp, children[i], keys[i])) {
				return false;
			}
			if (!key_check_greater_eq(bp, children[i + 1], keys[i])) {
				return false;
			}
		}
	}

	return true; 
}

bool key_check_less(BufferPool *bp, page_id_t node_id, int n) {
	if (node_id == INVALID_PAGE_ID) return true;
    void *raw = bp_fetch_page(bp, node_id);
    if (!raw) return false;
    Node *a = deserialize_node(raw);
    if (!a) { bp_unpin(bp, node_id, false); return false; }

	if (a->is_leaf) {
		for (int i = 0; i < a->num_keys; i ++) {
			if (a->keys[i] >= n) { 
                bp_unpin(bp, node_id, false);
                free(a);
				return false;
			}
		}
	} else {
		page_id_t children[MAX_KEYS + 1];
		int num_keys = a->num_keys;
		for (int i = 0; i <= num_keys; i++) {
			children[i] = a->data.inner.children[i];
		}
		bp_unpin(bp, node_id, false);
		free(a);

		for (int i = 0; i <= num_keys; i ++) {
			if (!key_check_less(bp, children[i], n)) {
				return false;
			}
		}
		return true;
	}
    bp_unpin(bp, node_id, false);
    free(a);
	return true;
}

bool key_check_greater_eq(BufferPool *bp, page_id_t node_id, int n) {
	if (node_id == INVALID_PAGE_ID) return true;
    void *raw = bp_fetch_page(bp, node_id);
    if (!raw) return false;
    Node *a = deserialize_node(raw);
    if (!a) { bp_unpin(bp, node_id, false); return false; }

	if (a->is_leaf) {
		for (int i = 0; i < a->num_keys; i ++) {
			if (a->keys[i] < n) { 
                bp_unpin(bp, node_id, false);
                free(a);
				return false;
			}
		}
	} else {
		page_id_t children[MAX_KEYS + 1];
		int num_keys = a->num_keys;
		for (int i = 0; i <= num_keys; i++) {
			children[i] = a->data.inner.children[i];
		}
		bp_unpin(bp, node_id, false);
		free(a);

		for (int i = 0; i <= num_keys; i ++) {
			if (!key_check_greater_eq(bp, children[i], n)) {
				return false;
			}
		}
		return true;
	}

    bp_unpin(bp, node_id, false);
    free(a);
	return true;
}

bool validate_tree(BufferPool *bp, page_id_t root_id) {
	if (root_id == INVALID_PAGE_ID) return true;
	int leaf_depth = -1;
	return helper_validate_tree(bp, root_id, 0, &leaf_depth);
}
