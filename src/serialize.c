#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "serialize.h"
#include "page_manager.h"

bool serialize_node(const Node *node, void *buffer) {
	if (!node || !buffer) {
		return false;
	}

	memset(buffer, 0, PAGE_SIZE);
	memcpy(buffer, node, sizeof(Node));
	return true;
}

Node *deserialize_node(const void *buffer) {
	if (!buffer) {
		return NULL;
	}

	Node *node = malloc(sizeof(Node));

	if (node) {
		memcpy(node, buffer, sizeof(Node));
	}
	
	return node;
}
