#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/bplustree.h"

/* ---------------------------------------------------------------
 * Pretty-print macros
 * --------------------------------------------------------------- */
#define PASS(name)          printf("[PASS] %s\n", (name))
#define FAIL(name, msg)     printf("[FAIL] %s: %s\n", (name), (msg))

/* Generic assertion: if cond is true → PASS, else → FAIL with message */
#define ASSERT(name, cond, msg) \
    do { if (cond) { PASS(name); } else { FAIL(name, msg); } } while (0)

/* Assert two int values are equal */
#define ASSERT_INT_EQ(name, expected, actual)                              \
    do {                                                                    \
        int _e = (expected), _a = (actual);                                \
        if (_e == _a) {                                                     \
            PASS(name);                                                     \
        } else {                                                            \
            char _buf[128];                                                 \
            snprintf(_buf, sizeof(_buf),                                    \
                     "expected %d, got %d", _e, _a);                       \
            FAIL(name, _buf);                                               \
        }                                                                   \
    } while (0)

/* Assert a pointer is NULL */
#define ASSERT_NULL(name, ptr)                                              \
    ASSERT(name, (ptr) == NULL, "expected NULL, got non-NULL")

/* Assert a pointer is NOT NULL */
#define ASSERT_NOT_NULL(name, ptr)                                          \
    ASSERT(name, (ptr) != NULL, "expected non-NULL, got NULL")

/* Assert two C-strings are equal */
#define ASSERT_STR_EQ(name, expected, actual)                              \
    do {                                                                    \
        const char *_e = (expected), *_a = (const char *)(actual);         \
        if (_a != NULL && strcmp(_e, _a) == 0) {                           \
            PASS(name);                                                     \
        } else {                                                            \
            char _buf[256];                                                 \
            snprintf(_buf, sizeof(_buf),                                    \
                     "expected \"%s\", got \"%s\"",                        \
                     _e, _a ? _a : "(null)");                              \
            FAIL(name, _buf);                                               \
        }                                                                   \
    } while (0)

/* ---------------------------------------------------------------
 * Helper: free a tree without freeing static string values
 * (use this when values point to string literals)
 * --------------------------------------------------------------- */
static void free_test_tree(Node *node) __attribute__((unused));
static void free_test_tree(Node *node) {
    if (!node) return;
    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++) {
            free_test_tree(fetch_node(node->data.inner.children[i]));
        }
    }
    
}

/* Helper: free a tree AND its heap-allocated values
 * (use this when values were malloc'd) */
static void free_test_tree_with_values(Node *node) __attribute__((unused));
static void free_test_tree_with_values(Node *node) {
    if (!node) return;
    if (node->is_leaf) {
        for (int i = 0; i < node->num_keys; i++) {
            
        }
    } else {
        for (int i = 0; i <= node->num_keys; i++) {
            free_test_tree_with_values(fetch_node(node->data.inner.children[i]));
        }
    }
    
}

#endif /* TEST_HELPERS_H */
