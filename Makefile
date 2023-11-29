# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

###############
## Docker Images.
GCC_BUILDER_IMAGE ?= gcc-builder:latest
GCC_TESTER_IMAGE ?= python-builder:latest
DOCKER_DIRS = simbus-sa modelc modelc-x86


################
## DSE C Schemas.
DSE_SCHEMA_REPO ?= https://github.com/boschglobal/dse.schemas
DSE_SCHEMA_VERSION ?= 1.0.0
export DSE_SCHEMA_URL ?= $(DSE_SCHEMA_REPO)/releases/download/v$(DSE_SCHEMA_VERSION)/dse-schemas.tar.gz


###############
## DSE C Library.
DSE_CLIB_REPO ?= https://github.com/boschglobal/dse.clib
DSE_CLIB_VERSION ?= 1.0.5
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
export SIMBUS_URI ?= redis://redis:6379
export REDIS_HOST ?= redis
export TESTER_CONTAINER_NAME = modelc_testenv


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

	DOCKER_TESTER_CMD := docker run -it --rm \
		--env CMAKE_TOOLCHAIN_FILE=/tmp/repo/extra/cmake/$(PACKAGE_ARCH).cmake \
		--env EXTERNAL_BUILD_DIR=$(EXTERNAL_BUILD_DIR) \
		--env PACKAGE_ARCH=$(PACKAGE_ARCH) \
		--env PACKAGE_VERSION=$(PACKAGE_VERSION) \
		--env PIP_EXTRA_INDEX_URL=$(PIP_EXTRA_INDEX_URL) \
		--env SIMBUS_URI=$(SIMBUS_URI) \
		--net dse \
		--volume $$(pwd):/tmp/repo \
		--volume $(EXTERNAL_BUILD_DIR):$(EXTERNAL_BUILD_DIR) \
		--volume ~/.ccache:/root/.ccache \
		--workdir /tmp/repo \
		$(GCC_TESTER_IMAGE)

	DOCKER_GDB_CMD := docker run -it --rm --name $(TESTER_CONTAINER_NAME) \
		--env CMAKE_TOOLCHAIN_FILE=/tmp/repo/extra/cmake/$(PACKAGE_ARCH).cmake \
		--env EXTERNAL_BUILD_DIR=$(EXTERNAL_BUILD_DIR) \
		--env PACKAGE_ARCH=$(PACKAGE_ARCH) \
		--env PACKAGE_VERSION=$(PACKAGE_VERSION) \
		--env PIP_EXTRA_INDEX_URL=$(PIP_EXTRA_INDEX_URL) \
		--env SIMBUS_URI=$(SIMBUS_URI) \
		--net dse \
		--volume $$(pwd):/tmp/repo \
		--volume $(EXTERNAL_BUILD_DIR):$(EXTERNAL_BUILD_DIR) \
		--volume ~/.ccache:/root/.ccache \
		--workdir /tmp/repo \
		$(GCC_BUILDER_IMAGE)

	DOCKER_TEST_SETUP_CMD :=  \
		docker network create dse; \
		docker run -d --rm --name redis --net dse -p 6379:6379 redis; \
		true

	DOCKER_TEST_TEARDOWN_CMD :=  \
		docker network inspect dse >/dev/null 2>&1 && docker network inspect -f '{{ range $$key, $$value := .Containers }}{{ printf "%s\n" $$key}}{{ end }}' dse | xargs -r docker kill; \
		docker network rm dse > /dev/null 2>&1; \
		true
endif


default: build

build:
	@${DOCKER_BUILDER_CMD} $(MAKE) do-build

package:
	@${DOCKER_BUILDER_CMD} $(MAKE) do-package

docker: build
	for d in $(DOCKER_DIRS) ;\
	do \
		docker build -f extra/docker/$$d/Dockerfile \
				--tag $$d:test . ;\
	done;

test_cmocka:
	@${DOCKER_BUILDER_CMD} $(MAKE) do-test_cmocka-build
	@${DOCKER_BUILDER_CMD} $(MAKE) do-test_cmocka-run

test_pytest:
	@${DOCKER_TEST_TEARDOWN_CMD}
	@${DOCKER_TEST_SETUP_CMD}
	@${DOCKER_TESTER_CMD} $(MAKE) do-test_pytest-run
	@${DOCKER_TEST_TEARDOWN_CMD}

test: test_cmocka test_pytest

test-env:
	@${DOCKER_TEST_TEARDOWN_CMD}
	@${DOCKER_TEST_SETUP_CMD}
	@${DOCKER_TESTER_CMD} /bin/bash
	@${DOCKER_TEST_TEARDOWN_CMD}

test-env-it:
	docker exec -it --workdir /tmp/repo $(TESTER_CONTAINER_NAME) /bin/bash

clean:
	@${DOCKER_TEST_TEARDOWN_CMD}
	@${DOCKER_BUILDER_CMD} $(MAKE) do-clean
	for d in $(DOCKER_IMAGES) ;\
	do \
		docker images -q $(DOCKER_PREFIX)-$$d | xargs -r docker rmi -f ;\
		docker images -q */*/$(DOCKER_PREFIX)-$$d | xargs -r docker rmi -f ;\
	done;
	docker images -qf dangling=true | xargs -r docker rmi

cleanall:
	@${DOCKER_BUILDER_CMD} $(MAKE) do-cleanall
	docker ps --filter status=dead --filter status=exited -aq | xargs -r docker rm -v
	docker images -qf dangling=true | xargs -r docker rmi
	docker volume ls -qf dangling=true | xargs -r docker volume rm

oss:
	@${DOCKER_BUILDER_CMD} $(MAKE) do-oss
	mkdir -p $(OSS_DIR)
	cp -r $(EXTERNAL_BUILD_DIR)/* $(OSS_DIR)

do-build:
	@for d in $(SUBDIRS); do ($(MAKE) -C $$d build ); done

do-package:
	@for d in $(SUBDIRS); do ($(MAKE) -C $$d package ); done

do-test_cmocka-build:
	$(MAKE) -C tests/cmocka build

do-test_cmocka-run:
	$(MAKE) -C tests/cmocka run

do-test_pytest-run:
	@-pip install -r tests/pytest/requirements.txt
	redis-cli -h $(REDIS_HOST) ping
	pytest -rx --asyncio-mode=strict -W ignore::DeprecationWarning tests/pytest

do-clean:
	@for d in $(SUBDIRS); do ($(MAKE) -C $$d clean ); done
	$(MAKE) -C tests/cmocka clean
	rm -rf $(OSS_DIR)
	rm -rvf *.zip
	rm -rvf *.log
	find . -name '__pycache__' -type d | xargs rm -fr

do-cleanall: do-clean
	@for d in $(SUBDIRS); do ($(MAKE) -C $$d cleanall ); done

do-oss:
	$(MAKE) -C extra/external oss

.PHONY: generate
generate:
	$(MAKE) -C doc generate

super-linter:
	docker run --rm --volume $$(pwd):/tmp/lint \
		--env RUN_LOCAL=true \
		--env DEFAULT_BRANCH=main \
		--env IGNORE_GITIGNORED_FILES=true \
		--env FILTER_REGEX_EXCLUDE="(dse/modelc/examples/apis/.*|doc/content/apis/modelc/examples/.*)" \
		--env VALIDATE_CPP=true \
		--env VALIDATE_YAML=true \
		github/super-linter:slim-v5


.PHONY: build docker test package clean cleanall oss generate super-linter \
		do-build do-test do-package do-clean do-cleanall do-oss
