# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

###############
## Docker Images.
GCC_BUILDER_IMAGE ?= ghcr.io/boschglobal/dse-gcc-builder:main
TESTSCRIPT_IMAGE ?= ghcr.io/boschglobal/dse-testscript:latest
SIMER_IMAGE ?= ghcr.io/boschglobal/dse-simer:latest


###############
## Docker Containers.
DOCKER_DIRS = simbus-sa modelc modelc-x86 testscript
TOOL_DIRS = simer


################
## DSE C Schemas.
DSE_SCHEMA_REPO ?= https://github.com/boschglobal/dse.schemas
DSE_SCHEMA_VERSION ?= 1.2.8
export DSE_SCHEMA_URL ?= $(DSE_SCHEMA_REPO)/releases/download/v$(DSE_SCHEMA_VERSION)/dse-schemas.tar.gz


###############
## DSE C Library.
DSE_CLIB_REPO ?= https://github.com/boschglobal/dse.clib
DSE_CLIB_VERSION ?= 1.0.10
export DSE_CLIB_URL ?= $(DSE_CLIB_REPO)/archive/refs/tags/v$(DSE_CLIB_VERSION).zip


###############
## Build parameters.
export NAMESPACE = dse
export MODULE = modelc
export EXTERNAL_BUILD_DIR ?= /tmp/$(NAMESPACE).$(MODULE)
export PACKAGE_ARCH ?= linux-amd64
export PACKAGE_ARCH_LIST ?= $(PACKAGE_ARCH)
export CMAKE_TOOLCHAIN_FILE ?= $(shell pwd -P)/extra/cmake/$(PACKAGE_ARCH).cmake
SUBDIRS = $(NAMESPACE)/$(MODULE)
export MODELC_SANDBOX_DIR ?= $(shell pwd -P)/dse/modelc/build/_out


###############
## Package parameters.
export PACKAGE_VERSION ?= 0.0.1
DIST_DIR := $(shell pwd -P)/$(NAMESPACE)/$(MODULE)/build/_dist
OSS_DIR = $(NAMESPACE)/__oss__
PACKAGE_DOC_NAME = DSE Model C Library
PACKAGE_NAME = ModelC
PACKAGE_NAME_LC = modelc
PACKAGE_PATH = $(NAMESPACE)/dist


###############
## Test Parameters.
export TESTSCRIPT_E2E_DIR ?= tests/testscript/e2e
TESTSCRIPT_E2E_FILES = \
	$(TESTSCRIPT_E2E_DIR)/minimal.txtar \
	$(TESTSCRIPT_E2E_DIR)/extended.txtar \
	$(TESTSCRIPT_E2E_DIR)/transform.txtar \
	$(TESTSCRIPT_E2E_DIR)/binary.txtar \
	$(TESTSCRIPT_E2E_DIR)/ncodec.txtar \
	$(TESTSCRIPT_E2E_DIR)/mstep.txtar \
	$(TESTSCRIPT_E2E_DIR)/transport.txtar \

#	$(TESTSCRIPT_E2E_DIR)/gateway.txtar \


ifneq ($(CI), true)
	DOCKER_BUILDER_CMD := docker run -it --rm \
		--env CMAKE_TOOLCHAIN_FILE=/tmp/repo/extra/cmake/$(PACKAGE_ARCH).cmake \
		--env EXTERNAL_BUILD_DIR=$(EXTERNAL_BUILD_DIR) \
		--env GDB_CMD="$(GDB_CMD)" \
		--env PACKAGE_ARCH=$(PACKAGE_ARCH) \
		--env PACKAGE_VERSION=$(PACKAGE_VERSION) \
		--volume $$(pwd):/tmp/repo \
		--volume $(EXTERNAL_BUILD_DIR):$(EXTERNAL_BUILD_DIR) \
		--volume ~/.ccache:/root/.ccache \
		--workdir /tmp/repo \
		$(GCC_BUILDER_IMAGE)
endif


default: build

.PHONY: build
build:
	@${DOCKER_BUILDER_CMD} $(MAKE) do-build

.PHONY: package
package:
	@${DOCKER_BUILDER_CMD} $(MAKE) do-package

.PHONY: simer
simer:
	mkdir -p extra/tools/simer/build/stage/bin
	mkdir -p extra/tools/simer/build/stage/lib
	mkdir -p extra/tools/simer/build/stage/lib32
	@if [ ${PACKAGE_ARCH} = "linux-amd64" ]; then \
		cp dse/modelc/build/_out/bin/simbus extra/tools/simer/build/stage/bin/simbus ;\
		cp dse/modelc/build/_out/bin/modelc extra/tools/simer/build/stage/bin/modelc ;\
		cp dse/modelc/build/_out/lib/mcl_model.so extra/tools/simer/build/stage/lib/mcl_model.so ;\
		cp -r licenses -t extra/tools/simer/build/stage ;\
	fi
	@if [ ${PACKAGE_ARCH} = "linux-x86" ]; then \
		cp dse/modelc/build/_out/bin/modelc extra/tools/simer/build/stage/bin/modelc32 ;\
		cp dse/modelc/build/_out/lib/mcl_model.so extra/tools/simer/build/stage/lib32/mcl_model.so ;\
		cp dse/modelc/build/_out/examples/simer/lib/libcounter.so extra/tools/simer/build/stage/lib32/libcounter.so ;\
	fi

.PHONY: docker
docker:
	for d in $(DOCKER_DIRS) ;\
	do \
		docker build -f extra/docker/$$d/Dockerfile \
				--tag $$d:test . ;\
	done;

.PHONY: tools
tools:
	for d in $(TOOL_DIRS) ;\
	do \
		docker build -f extra/tools/$$d/build/package/Dockerfile \
				--tag $$d:test extra/tools/$$d ;\
	done;

.PHONY: test_cmocka
test_cmocka:
	@${DOCKER_BUILDER_CMD} $(MAKE) do-test_cmocka-build
	@${DOCKER_BUILDER_CMD} $(MAKE) do-test_cmocka-run

.PHONY: test_e2e
test_e2e: do-test_testscript-e2e

.PHONY: test
test: test_cmocka test_e2e

.PHONY: clean
clean:
	@${DOCKER_BUILDER_CMD} $(MAKE) do-clean
	for d in $(DOCKER_IMAGES) ;\
	do \
		docker images -q $(DOCKER_PREFIX)-$$d | xargs -r docker rmi -f ;\
		docker images -q */*/$(DOCKER_PREFIX)-$$d | xargs -r docker rmi -f ;\
	done;
	docker images -qf dangling=true | xargs -r docker rmi

.PHONY: cleanall
cleanall:
	@${DOCKER_BUILDER_CMD} $(MAKE) do-cleanall
	docker ps --filter status=dead --filter status=exited -aq | xargs -r docker rm -v
	docker images -qf dangling=true | xargs -r docker rmi
	docker volume ls -qf dangling=true | xargs -r docker volume rm

.PHONY: oss
oss:
	@${DOCKER_BUILDER_CMD} $(MAKE) do-oss

.PHONY: do-build
do-build:
	@for d in $(SUBDIRS); do ($(MAKE) -C $$d build ); done

.PHONY: do-package
do-package:
	@for d in $(SUBDIRS); do ($(MAKE) -C $$d package ); done

do-test_cmocka-build:
	$(MAKE) -C tests/cmocka build

do-test_cmocka-run:
	$(MAKE) -C tests/cmocka run

do-test_testscript-e2e:
# Test debug; add '-v' to Testscript command (e.g. $(TESTSCRIPT_IMAGE) -v \).
ifeq ($(PACKAGE_ARCH), linux-amd64)
	@set -eu; for t in $(TESTSCRIPT_E2E_FILES) ;\
	do \
		echo "Running E2E Test: $$t" ;\
		docker run -i --rm \
			-e ENTRYDIR=$$(pwd) \
			-v /var/run/docker.sock:/var/run/docker.sock \
			-v $$(pwd):/repo \
			$(TESTSCRIPT_IMAGE) \
				-e ENTRYDIR=$$(pwd) \
				-e SIMER=$(SIMER_IMAGE) \
				$$t ;\
	done;
endif

.PHONY: do-clean
do-clean:
	@for d in $(SUBDIRS); do ($(MAKE) -C $$d clean ); done
	$(MAKE) -C tests/cmocka clean
	$(MAKE) -C tests/pytest/benchmark clean
	rm -rf $(OSS_DIR)
	rm -rvf *.zip
	rm -rvf *.log
	find . -name '__pycache__' -type d | xargs rm -fr

.PHONY: do-cleanall
do-cleanall: do-clean
	@for d in $(SUBDIRS); do ($(MAKE) -C $$d cleanall ); done

.PHONY: do-oss
do-oss:
	$(MAKE) -C extra/external oss

.PHONY: generate
generate:
	$(MAKE) -C doc generate

.PHONY: super-linter
super-linter:
	docker run --rm --volume $$(pwd):/tmp/lint \
		--env RUN_LOCAL=true \
		--env DEFAULT_BRANCH=main \
		--env IGNORE_GITIGNORED_FILES=true \
		--env FILTER_REGEX_EXCLUDE="(dse/mocks/examples/.*|dse/modelc/examples/doc/.*|doc/content/apis/modelc/examples/.*|doc/content/docs/examples/modelc/.*)" \
		--env VALIDATE_CPP=true \
		--env VALIDATE_YAML=true \
		--env VALIDATE_PYTHON_PYLINT=true \
		--env VALIDATE_PYTHON_FLAKE8=true \
		github/super-linter:slim-v5

