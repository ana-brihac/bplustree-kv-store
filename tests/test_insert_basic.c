/*
 * test_insert_basic.c
 *
 * Tests for: insert — basic behavior (empty tree, sort order, duplicates)
 *
 * Cases:
 *   insert_empty_returns_non_null    - inserting into NULL returns a valid node
 *   insert_empty_is_leaf             - first node is a leaf
 *   insert_empty_num_keys_one        - one key after first insert
 *   insert_empty_key_stored          - the correct key is stored at index 0
 *   insert_empty_value_stored        - the correct value is stored at index 0
 *   insert_sort_num_keys             - three inserts → three keys
 *   insert_sort_order_keys           - keys are sorted ascending after inserts
 *   insert_sort_values_follow        - values follow their keys after sorting
 *   insert_duplicate_num_keys        - duplicate key does not increase num_keys
 *   insert_duplicate_value_unchanged - duplicate key does not overwrite existing value
 */

#include "test_helpers.h"
#include "../src/buffer_pool.h"
#include "../src/page_manager.h"
#include "../src/serialize.h"

static void test_insert_empty(void) {
    remove("test_insert_empty.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_insert_empty.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    root_id = insert(tree, 10, "ten");

    ASSERT_NOT_NULL("insert_empty_returns_valid_id", root_id != INVALID_PAGE_ID ? (void*)1 : NULL);
    void *raw = bp_fetch_page(bp, root_id);
    Node *root = deserialize_node(raw);
    
    ASSERT("insert_empty_is_leaf",   root->is_leaf == true, "should be a leaf");
    ASSERT_INT_EQ("insert_empty_num_keys_one", 1, root->num_keys);
    ASSERT_INT_EQ("insert_empty_key_stored",   10, root->keys[0]);
    ASSERT_STR_EQ("insert_empty_value_stored", "ten", (char*)root->data.leaf.values[0]);

    bp_unpin(bp, root_id, false);
    free(root);
    tree_close(tree);
        remove("test_insert_empty.db");
}

static void test_insert_sort_order(void) {
    remove("test_insert_sort.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_insert_sort.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    root_id = insert(tree, 20, "twenty");
    root_id = insert(tree, 5,  "five");
    root_id = insert(tree, 10, "ten");

    void *raw = bp_fetch_page(bp, root_id);
    Node *root = deserialize_node(raw);

    ASSERT_INT_EQ("insert_sort_num_keys", 3, root->num_keys);
    ASSERT_INT_EQ("insert_sort_order_key0",  5,  root->keys[0]);
    ASSERT_INT_EQ("insert_sort_order_key1", 10,  root->keys[1]);
    ASSERT_INT_EQ("insert_sort_order_key2", 20,  root->keys[2]);
    ASSERT_STR_EQ("insert_sort_value0", "five",   (char*)root->data.leaf.values[0]);
    ASSERT_STR_EQ("insert_sort_value1", "ten",    (char*)root->data.leaf.values[1]);
    ASSERT_STR_EQ("insert_sort_value2", "twenty", (char*)root->data.leaf.values[2]);

    bp_unpin(bp, root_id, false);
    free(root);
    tree_close(tree);
        remove("test_insert_sort.db");
}

static void test_insert_duplicate(void) {
    remove("test_insert_dup.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_insert_dup.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    root_id = insert(tree, 10, "ten");
    root_id = insert(tree, 10, "duplicate");

    void *raw = bp_fetch_page(bp, root_id);
    Node *root = deserialize_node(raw);

    ASSERT_INT_EQ("insert_duplicate_num_keys",         1,   root->num_keys);
    ASSERT_STR_EQ("insert_duplicate_value_unchanged", "ten", (char*)root->data.leaf.values[0]);

    bp_unpin(bp, root_id, false);
    free(root);
    tree_close(tree);
        remove("test_insert_dup.db");
}

int main(void) {
    test_insert_empty();
    test_insert_sort_order();
    test_insert_duplicate();
    return 0;
}
