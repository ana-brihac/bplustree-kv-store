CC     = gcc
CFLAGS = -Wall -Wextra -g -fsanitize=address
SRC    = src/bplustree.c src/page_manager.c src/buffer_pool.c src/serialize.c

# ---------- test binaries ----------
TESTS = test_create \
        test_insert_basic \
        test_insert_no_split \
        test_validate \
        test_insert_into_leaf_sorted \
        test_insert_with_split \
        test_split_leaf \
        test_search_leaf \
        test_search_tree \
        test_delete \
        test_range_search \
        test_page_manager \
        test_buffer_pool \
        test_wal

BIN_DIR     = tests/bin
RESULT_DIR  = tests/results
EXPECT_DIR  = tests/expected

BINS = $(addprefix $(BIN_DIR)/, $(TESTS))

# ---------- build ----------
all: $(BINS)

$(BIN_DIR)/test_buffer_pool: tests/test_buffer_pool.c src/buffer_pool.c src/page_manager.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -I. -o $@ src/buffer_pool.c src/page_manager.c $<

$(BIN_DIR)/test_wal: tests/test_wal.c src/wal.c src/page_manager.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -I. -o $@ src/wal.c src/page_manager.c $<

$(BIN_DIR)/%: tests/%.c $(SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I. -o $@ $(SRC) $<

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# ---------- test ----------
test: all | $(RESULT_DIR)
	@PASS=0; FAIL=0; \
	for t in $(TESTS); do \
		bin="$(BIN_DIR)/$$t"; \
		actual="$(RESULT_DIR)/$$t.actual"; \
		expected="$(EXPECT_DIR)/$$t.expected"; \
		$$bin > "$$actual" 2>"$(RESULT_DIR)/$$t.err"; \
		exit_code=$$?; \
		if [ $$exit_code -ne 0 ]; then \
			printf "%-35s [\033[0;31mFAIL\033[0m] (crash / ASAN error)\n" "$$t"; \
			cat "$(RESULT_DIR)/$$t.err"; \
			FAIL=$$((FAIL + 1)); \
		elif diff -q "$$expected" "$$actual" > /dev/null 2>&1; then \
			printf "%-35s [\033[0;32mPASS\033[0m]\n" "$$t"; \
			PASS=$$((PASS + 1)); \
		else \
			printf "%-35s [\033[0;31mFAIL\033[0m] (output mismatch)\n" "$$t"; \
			echo "  --- expected"; \
			diff "$$expected" "$$actual" | grep '^[<>]' | sed 's/^/  /'; \
			FAIL=$$((FAIL + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "Results: $$PASS passed, $$FAIL failed out of $$((PASS + FAIL)) test suites"

$(RESULT_DIR):
	mkdir -p $(RESULT_DIR)

# ---------- clean ----------
clean:
	rm -f $(BINS) $(RESULT_DIR)/*.actual $(RESULT_DIR)/*.err