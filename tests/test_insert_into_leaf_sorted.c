/*
 * test_insert_into_leaf_sorted.c
 *
 * Tests for: insert_into_leaf_sorted
 *
 * Cases:
 *   sorted_single_key              - one key inserted, num_keys is 1
 *   sorted_key_stored_correctly    - key is stored at index 0
 *   sorted_value_stored_correctly  - value is stored at index 0
 *   sorted_ascending_order         - keys stay sorted after out-of-order inserts
 *   sorted_values_follow_keys      - values follow their keys after reordering
 *   sorted_prepend_key             - inserting smallest key shifts others right
 *   sorted_middle_key              - inserting middle key shifts only right side
 *   sorted_duplicate_rejected      - duplicate key does not increase num_keys
 *   sorted_duplicate_value_kept    - original value is kept on duplicate insert
 *   sorted_four_keys_full          - filling to MAX_KEYS works correctly
 */

#include "test_helpers.h"
#include "../src/buffer_pool.h"
#include "../src/page_manager.h"
#include "../src/serialize.h"

static void test_single_insert(void) {
    PageManager *pm = pm_open("test_leaf1.db");
    BufferPool *bp = bp_create(pm, "test_wal.log");
    page_id_t leaf_id = create_leaf_node(bp);
    void *raw = bp_fetch_page(bp, leaf_id);
    Node *leaf = deserialize_node(raw);
    
    insert_into_leaf_sorted(leaf, 42, "forty-two");

    ASSERT_INT_EQ("sorted_single_key",             1,          leaf->num_keys);
    ASSERT_INT_EQ("sorted_key_stored_correctly",   42,         leaf->keys[0]);
    ASSERT_STR_EQ("sorted_value_stored_correctly", "forty-two", (char*)leaf->data.leaf.values[0]);

    serialize_node(leaf, raw);
    bp_unpin(bp, leaf_id, true);
    free(leaf);
    bp_destroy(bp);
    pm_close(pm);
    remove("test_leaf1.db");
}

static void test_out_of_order_inserts(void) {
    PageManager *pm = pm_open("test_leaf2.db");
    BufferPool *bp = bp_create(pm, "test_wal.log");
    page_id_t leaf_id = create_leaf_node(bp);
    void *raw = bp_fetch_page(bp, leaf_id);
    Node *leaf = deserialize_node(raw);
    
    insert_into_leaf_sorted(leaf, 30, "thirty");
    insert_into_leaf_sorted(leaf, 10, "ten");
    insert_into_leaf_sorted(leaf, 20, "twenty");

    ASSERT_INT_EQ("sorted_ascending_key0", 10, leaf->keys[0]);
    ASSERT_INT_EQ("sorted_ascending_key1", 20, leaf->keys[1]);
    ASSERT_INT_EQ("sorted_ascending_key2", 30, leaf->keys[2]);
    ASSERT_STR_EQ("sorted_values_follow_key0", "ten",    (char*)leaf->data.leaf.values[0]);
    ASSERT_STR_EQ("sorted_values_follow_key1", "twenty", (char*)leaf->data.leaf.values[1]);
    ASSERT_STR_EQ("sorted_values_follow_key2", "thirty", (char*)leaf->data.leaf.values[2]);

    serialize_node(leaf, raw);
    bp_unpin(bp, leaf_id, true);
    free(leaf);
    bp_destroy(bp);
    pm_close(pm);
    remove("test_leaf2.db");
}

static void test_prepend(void) {
    PageManager *pm = pm_open("test_leaf3.db");
    BufferPool *bp = bp_create(pm, "test_wal.log");
    page_id_t leaf_id = create_leaf_node(bp);
    void *raw = bp_fetch_page(bp, leaf_id);
    Node *leaf = deserialize_node(raw);
    
    insert_into_leaf_sorted(leaf, 20, "twenty");
    insert_into_leaf_sorted(leaf, 30, "thirty");
    insert_into_leaf_sorted(leaf, 5,  "five");

    ASSERT_INT_EQ("sorted_prepend_key",  5, leaf->keys[0]);
    ASSERT_INT_EQ("sorted_prepend_key1", 20, leaf->keys[1]);
    ASSERT_INT_EQ("sorted_prepend_key2", 30, leaf->keys[2]);

    serialize_node(leaf, raw);
    bp_unpin(bp, leaf_id, true);
    free(leaf);
    bp_destroy(bp);
    pm_close(pm);
    remove("test_leaf3.db");
}

static void test_middle_insert(void) {
    PageManager *pm = pm_open("test_leaf4.db");
    BufferPool *bp = bp_create(pm, "test_wal.log");
    page_id_t leaf_id = create_leaf_node(bp);
    void *raw = bp_fetch_page(bp, leaf_id);
    Node *leaf = deserialize_node(raw);
    
    insert_into_leaf_sorted(leaf, 10, "ten");
    insert_into_leaf_sorted(leaf, 30, "thirty");
    insert_into_leaf_sorted(leaf, 20, "twenty");

    ASSERT_INT_EQ("sorted_middle_key0", 10, leaf->keys[0]);
    ASSERT_INT_EQ("sorted_middle_key1", 20, leaf->keys[1]);
    ASSERT_INT_EQ("sorted_middle_key2", 30, leaf->keys[2]);

    serialize_node(leaf, raw);
    bp_unpin(bp, leaf_id, true);
    free(leaf);
    bp_destroy(bp);
    pm_close(pm);
    remove("test_leaf4.db");
}

static void test_duplicate_rejected(void) {
    PageManager *pm = pm_open("test_leaf5.db");
    BufferPool *bp = bp_create(pm, "test_wal.log");
    page_id_t leaf_id = create_leaf_node(bp);
    void *raw = bp_fetch_page(bp, leaf_id);
    Node *leaf = deserialize_node(raw);
    
    insert_into_leaf_sorted(leaf, 10, "original");
    insert_into_leaf_sorted(leaf, 10, "duplicate");

    ASSERT_INT_EQ("sorted_duplicate_rejected",   1,          leaf->num_keys);
    ASSERT_STR_EQ("sorted_duplicate_value_kept", "original", (char*)leaf->data.leaf.values[0]);

    serialize_node(leaf, raw);
    bp_unpin(bp, leaf_id, true);
    free(leaf);
    bp_destroy(bp);
    pm_close(pm);
    remove("test_leaf5.db");
}

static void test_fill_to_max(void) {
    PageManager *pm = pm_open("test_leaf6.db");
    BufferPool *bp = bp_create(pm, "test_wal.log");
    page_id_t leaf_id = create_leaf_node(bp);
    void *raw = bp_fetch_page(bp, leaf_id);
    Node *leaf = deserialize_node(raw);
    
    insert_into_leaf_sorted(leaf, 1,  "one");
    insert_into_leaf_sorted(leaf, 2,  "two");
    insert_into_leaf_sorted(leaf, 3,  "three");
    insert_into_leaf_sorted(leaf, 4,  "four");

    ASSERT_INT_EQ("sorted_four_keys_full",   4, leaf->num_keys);
    ASSERT_INT_EQ("sorted_four_keys_key3",   4,        leaf->keys[3]);
    ASSERT_STR_EQ("sorted_four_keys_value3", "four",   (char*)leaf->data.leaf.values[3]);

    serialize_node(leaf, raw);
    bp_unpin(bp, leaf_id, true);
    free(leaf);
    bp_destroy(bp);
    pm_close(pm);
    remove("test_leaf6.db");
}

int main(void) {
	test_single_insert();
	test_out_of_order_inserts();
	test_prepend();
	test_middle_insert();
	test_duplicate_rejected();
	test_fill_to_max();
	return 0;
}
