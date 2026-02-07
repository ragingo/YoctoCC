# ==================================================
#  テスト
# ==================================================

include mk/common.mk

test:
	@echo "Running parallel test suite..."
	@bash test/run_tests_parallel.sh
