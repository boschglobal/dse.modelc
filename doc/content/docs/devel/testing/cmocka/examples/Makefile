export NAMESPACE = fsil
export MODULE = runnable
export PROJECT_URL ?= $(GITHUB_ORG)/$(NAMESPACE).$(MODULE).git
export DOCKER_REPO ?= ghcr.io/boschglobal
export EXTERNAL_BUILD_DIR ?= /tmp/$(NAMESPACE).$(MODULE)
export PACKAGE_ARCH ?= linux-amd64
export CMAKE_TOOLCHAIN_FILE ?= $(shell pwd -P)/extra/cmake/$(PACKAGE_ARCH).cmake
export PACKAGE_VERSION ?= 0.0.2
export GCC_BUILDER_IMAGE ?= $(DOCKER_REPO)/dse-gcc-builder:main

ifneq ($(CI), true)
	DOCKER_BUILDER_CMD := docker run -it --rm \
		--volume $$(pwd):/tmp/repo \
		--volume $(EXTERNAL_BUILD_DIR):$(EXTERNAL_BUILD_DIR) \
		--volume ~/.ccache:/root/.ccache \
		--env CMAKE_TOOLCHAIN_FILE=/tmp/repo/extra/cmake/$(PACKAGE_ARCH).cmake \
		--env EXTERNAL_BUILD_DIR=$(EXTERNAL_BUILD_DIR) \
		--env PACKAGE_ARCH=$(PACKAGE_ARCH) \
		--env HTTP_PROXY=$(http_proxy) \
		--env HTTPS_PROXY=$(https_proxy) \
		--env NO_PROXY=$(no_proxy) \
		--env AR_USER=$(AR_USER) \
		--env AR_TOKEN=$(AR_TOKEN) \
		--env GHE_USER=$(GHE_USER) \
		--env GHE_TOKEN=$(GHE_TOKEN) \
		--env DOCKER_REPO=$(DOCKER_REPO) \
		--env PROJECT_URL=$(PROJECT_URL) \
		--env GDB_CMD="$(GDB_CMD)" \
		--env PACKAGE_VERSION=$(PACKAGE_VERSION) \
		--env NAMESPACE=$(NAMESPACE) \
		--env MODULE=$(MODULE) \
		--workdir /tmp/repo \
		$(GCC_BUILDER_IMAGE)
endif

do-test_cmocka-build:
	$(MAKE) -C tests/cmocka build
do-test_cmocka-run:
	$(MAKE) -C tests/cmocka run

.PHONY: test_cmocka
test_cmocka:
	@${DOCKER_BUILDER_CMD} $(MAKE) do-test_cmocka-build
	@${DOCKER_BUILDER_CMD} $(MAKE) do-test_cmocka-run

.PHONY: test
test: test_cmocka
