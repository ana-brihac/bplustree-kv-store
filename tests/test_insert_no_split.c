#include "test_helpers.h"
#include "../src/buffer_pool.h"
#include "../src/page_manager.h"
#include "../src/serialize.h"

static void test_fill_three(void) {
    remove("test_fill1.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_fill1.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    root_id = insert(tree, 10, "v1");
    root_id = insert(tree, 20, "v2");
    root_id = insert(tree, 30, "v3");

    void *raw = bp_fetch_page(bp, root_id);
    Node *root = deserialize_node(raw);

    ASSERT_INT_EQ("fill_leaf_num_keys_3", 3, root->num_keys);
    ASSERT("fill_leaf_still_leaf", root->is_leaf == true, "should still be a leaf");
    ASSERT_INT_EQ("fill_leaf_key0", 10, root->keys[0]);
    ASSERT_INT_EQ("fill_leaf_key1", 20, root->keys[1]);
    ASSERT_INT_EQ("fill_leaf_key2", 30, root->keys[2]);

    bp_unpin(bp, root_id, false);
    free(root);
    tree_close(tree);
        remove("test_fill1.db");
}

static void test_fill_max_minus_one(void) {
    remove("test_fill2.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_fill2.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t root_id = INVALID_PAGE_ID;
    root_id = insert(tree, 100, "a");
    root_id = insert(tree, 50,  "b");
    root_id = insert(tree,  75, "c");

    void *raw = bp_fetch_page(bp, root_id);
    Node *root = deserialize_node(raw);

    ASSERT_INT_EQ("fill_leaf_max_num_keys",   3, root->num_keys);
    ASSERT("fill_leaf_max_still_leaf", root->is_leaf == true, "should still be a leaf");

    bp_unpin(bp, root_id, false);
    free(root);
    tree_close(tree);
        remove("test_fill2.db");
}

static void test_fill_and_search(void) {
    remove("test_fill3.db");
    remove("test_wal.log");
    Tree *tree = tree_open("test_fill3.db", "test_wal.log");
    PageManager *pm = tree->pm;
    BufferPool *bp = tree->bp;
    page_id_t leaf_id = create_leaf_node(bp);
    tree->root_id = leaf_id;
    void *raw = bp_fetch_page(bp, leaf_id);
    Node *leaf = deserialize_node(raw);

    insert_into_leaf_sorted(leaf, 5,  "five");
    insert_into_leaf_sorted(leaf, 15, "fifteen");
    insert_into_leaf_sorted(leaf, 25, "twenty-five");
    insert_into_leaf_sorted(leaf, 35, "thirty-five");

    ASSERT_INT_EQ("fill_full_leaf_num_keys", MAX_KEYS, leaf->num_keys);
    serialize_node(leaf, raw);
    bp_unpin(bp, leaf_id, true);
    free(leaf);

    ASSERT_STR_EQ("fill_full_search_mid", "fifteen", (char*)search(tree, 15));
    ASSERT_STR_EQ("fill_full_search_last", "thirty-five", (char*)search(tree, 35));
    ASSERT_NULL("fill_full_search_missing", search(tree, 99));

    tree_close(tree);
        remove("test_fill3.db");
}

int main(void) {
    test_fill_three();
    test_fill_max_minus_one();
    test_fill_and_search();
    return 0;
}
