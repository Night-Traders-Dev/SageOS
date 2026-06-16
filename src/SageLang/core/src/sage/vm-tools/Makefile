# ----------------------------------------------------------------------------
# SageVM Makefile (Orchestrator for sagemake)
# ----------------------------------------------------------------------------

all:
	@python3 sagemake

install:
	@python3 sagemake --install

debug:
	@python3 sagemake --debug

rebuild-host:
	@python3 sagemake --rebuild-sage

clean:
	@rm -f sgvm sgvmc sagevm tests/*.sgvm
	@if [ -d ".deps/SageLang/core" ]; then \
		$(MAKE) -C .deps/SageLang/core clean; \
	fi

test: all
	@python3 tests/run_tests.py
