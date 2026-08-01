#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "test_helpers.h"
// Helper function to insert array of keys
void insert_keys(Node **root, int keys[], char *vals[], int n) {
	for (int i = 0; i < n; i++) {
		*root = insert(*root, keys[i], vals[i]);
	}
}

void test_basic_range() {
	Node *root = NULL;
	int keys[] = {10, 20, 30, 40, 50, 60, 70, 80};
	char *vals[] = {"1", "2", "3", "4", "5", "6", "7", "8"};
	insert_keys(&root, keys, vals, 8);

	int res_keys[20];
	void *res_vals[20];
	int num_res = 0;

	bool found = range_search(root, 25, 65, res_keys, res_vals, &num_res);
	
	assert(found == true);
	assert(num_res == 4);
	assert(res_keys[0] == 30);
	assert(res_keys[1] == 40);
	assert(res_keys[2] == 50);
	assert(res_keys[3] == 60);
	
	printf("[PASS] test_basic_range\n");
	free_test_tree(root);
}

void test_range_outside() {
	Node *root = NULL;
	int keys[] = {10, 20, 30};
	char *vals[] = {"1", "2", "3"};
	insert_keys(&root, keys, vals, 3);

	int res_keys[20];
	void *res_vals[20];
	int num_res = 0;

	range_search(root, 50, 100, res_keys, res_vals, &num_res);
	
	assert(num_res == 0);
	
	printf("[PASS] test_range_outside\n");
	free_test_tree(root);
}

void test_range_all() {
	Node *root = NULL;
	int keys[] = {10, 20, 30, 40, 50};
	char *vals[] = {"1", "2", "3", "4", "5"};
	insert_keys(&root, keys, vals, 5);

	int res_keys[20];
	void *res_vals[20];
	int num_res = 0;

	range_search(root, 0, 100, res_keys, res_vals, &num_res);
	
	assert(num_res == 5);
	for (int i = 0; i < 5; i++) {
		assert(res_keys[i] == keys[i]);
	}
	
	printf("[PASS] test_range_all\n");
	free_test_tree(root);
}

void test_range_empty() {
	Node *root = NULL;
	int res_keys[20];
	void *res_vals[20];
	int num_res = 0;

	bool found = range_search(root, 10, 20, res_keys, res_vals, &num_res);
	assert(found == false);
	assert(num_res == 0);
	
	printf("[PASS] test_range_empty\n");
}

int main() {
	test_basic_range();
	test_range_outside();
	test_range_all();
	test_range_empty();
	
	printf("All range_search tests passed.\n");
	return 0;
}
