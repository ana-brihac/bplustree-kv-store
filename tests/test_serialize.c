#include "test_helpers.h"
#include "../src/serialize.h"
#include <string.h>

void test_serialize_leaf(void) {
    Node leaf;
    memset(&leaf, 0, sizeof(Node));
    leaf.page_id = 42;
    leaf.parent_id = 10;
    leaf.num_keys = 2;
    leaf.is_leaf = true;
    leaf.keys[0] = 5;
    leaf.keys[1] = 10;
    leaf.data.leaf.values[0] = 100;
    leaf.data.leaf.values[1] = 200;
    leaf.data.leaf.next_id = 99;

    char buffer[4096]; // PAGE_SIZE is typically 4096
    bool success = serialize_node(&leaf, buffer);
    ASSERT("serialize_leaf_success", success, "Serialization should succeed");

    Node *deserialized = deserialize_node(buffer);
    ASSERT("deserialize_leaf_not_null", deserialized != NULL, "Deserialized node should not be NULL");
    
    ASSERT("deserialize_leaf_page_id", deserialized->page_id == 42, "Page ID should match");
    ASSERT("deserialize_leaf_parent_id", deserialized->parent_id == 10, "Parent ID should match");
    ASSERT("deserialize_leaf_num_keys", deserialized->num_keys == 2, "Num keys should match");
    ASSERT("deserialize_leaf_is_leaf", deserialized->is_leaf == true, "Is leaf should match");
    ASSERT("deserialize_leaf_key_0", deserialized->keys[0] == 5, "Key 0 should match");
    ASSERT("deserialize_leaf_key_1", deserialized->keys[1] == 10, "Key 1 should match");
    ASSERT("deserialize_leaf_value_0", deserialized->data.leaf.values[0] == 100, "Value 0 should match");
    ASSERT("deserialize_leaf_value_1", deserialized->data.leaf.values[1] == 200, "Value 1 should match");
    ASSERT("deserialize_leaf_next_id", deserialized->data.leaf.next_id == 99, "Next ID should match");
    
    free(deserialized);
}

int main(void) {
    test_serialize_leaf();
    return 0;
}
