# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

export NAMESPACE = dse
export MODULE = modelc
export EXTERNAL_BUILD_DIR ?= /tmp/$(NAMESPACE).$(MODULE)
export REPO_MIRRORS ?= $(shell pwd -P)/../../repo_mirrors.cmake
export PACKAGE_ARCH ?= linux-amd64
export CMAKE_TOOLCHAIN_FILE ?= $(shell pwd -P)/../../extra/cmake/$(PACKAGE_ARCH).cmake
export PROJECT_URL ?= https://github.com/boschglobal/$(NAMESPACE).$(MODULE).git
export PACKAGE_VERSION ?= 0.0.2
export MAKE_NPROC ?= $(shell nproc)


SUBDIRS = adapter controller common


default: build

external:
	$(MAKE) -j $(MAKE_NPROC) -C ../../extra/external

build: external
# Build from scratch if no build dir.
	if [ ! -d "build" ]; then \
		mkdir -p build; \
		cd build; \
		cmake -DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE) .. ; \
	fi
# Build incremental.
	cd build; cmake --build . -j $(MAKE_NPROC)
	cd build; cmake --build . -j $(MAKE_NPROC) -t install
	@echo ""
	@echo "Sandbox files: - $$(pwd)/build/_out"
	@echo "--------------"
	@find build/_out/ -type f -name '*' -exec ls -sh --color=auto {} \;

package:
	@echo "Package parameters:"
	@echo "-------------------"
	@echo "NAMESPACE : $(NAMESPACE)"
	@echo "MODULE    : $(MODULE)"
	@echo "VERSION   : $(PACKAGE_VERSION)"
	@echo "ARCH      : $(PACKAGE_ARCH)"
	cd build; make package
	@echo ""
	@echo "Package files: - $$(pwd)/build/_dist"
	@echo "--------------"
	@find build/_dist/ -type f -name '*' -exec ls -sh --color=auto {} \;

clean:
	@-for d in $(SUBDIRS); do ($(MAKE) -C $$d $@ ); done
	$(MAKE) -j $(MAKE_NPROC) -C ../../extra/external clean
	rm -rf build

cleanall: clean
	$(MAKE) -j $(MAKE_NPROC) -C ../../extra/external cleanall

.PHONY: default build external package clean cleanall
