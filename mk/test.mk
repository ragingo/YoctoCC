# ==================================================
#  テスト
# ==================================================

include mk/common.mk
include mk/output.mk

# FORMAT=simple (default) | md
FORMAT ?= simple

test: $(COMPILER) $(TEST_HELPER_O)
	@echo "Running parallel test suite..."
	@FORMAT=$(FORMAT) python3 test/run_tests_parallel.sh $(FILTERS)

