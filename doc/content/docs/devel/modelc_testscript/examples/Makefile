
###############
## Docker Images.
TESTSCRIPT_IMAGE ?= ghcr.io/boschglobal/dse-testscript:main
SIMER_IMAGE ?= ghcr.io/boschglobal/dse-simer:main


###############
## Test Parameters.
export TESTSCRIPT_E2E_DIR ?= tests/testscript/e2e
TESTSCRIPT_E2E_FILES = \
	$(TESTSCRIPT_E2E_DIR)/exec.txtar \
	$(TESTSCRIPT_E2E_DIR)/simer.txtar \


.PHONY: test
test: test_e2e

.PHONY: test_e2e
test_e2e: do-test_testscript-e2e

do-test_testscript-e2e:
# Test debug; add '-v' to Testscript command (e.g. $(TESTSCRIPT_IMAGE) -v \).
ifeq ($(PACKAGE_ARCH), linux-amd64)
	@set -eu; for t in $(TESTSCRIPT_E2E_FILES) ;\
	do \
		echo "Running E2E Test: $$t" ;\
		docker run -it --rm \
			-e ENTRYDIR=$$(pwd) \
			-v /var/run/docker.sock:/var/run/docker.sock \
			-v $$(pwd):/repo \
			$(TESTSCRIPT_IMAGE) \
				-e ENTRYDIR=$$(pwd) \
				-e SIMER=$(SIMER_IMAGE) \
				$$t ;\
	done;
endif

