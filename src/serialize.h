#ifndef SERIALIZE_H
#define SERIALIZE_H

#include "bplustree.h"

// Converts an in-memory Node into a PAGE_SIZE byte buffer.
// Returns false if node doesn't fit (shouldn't happen if MAX_KEYS is sized correctly).
bool serialize_node(const Node *node, void *buffer);

// Converts a PAGE_SIZE byte buffer back into a heap-allocated Node.
// Caller is responsible for freeing the returned Node.
Node *deserialize_node(const void *buffer);

#endif