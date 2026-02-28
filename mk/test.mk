# ==================================================
#  テスト
# ==================================================

include mk/common.mk

test:
	@echo "Running parallel test suite..."
	@CC="$(CC)" bash test/run_tests_parallel.sh
