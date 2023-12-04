# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

export EXTERNAL_BUILD_DIR ?= /dev/null


default: build

setup:
	mkdir build;
	cd build; cmake -DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE) ..

build:
# Force the build if build dir missing.
	if [ ! -d $(EXTERNAL_BUILD_DIR) ]; then rm -rf build; fi
# Force the build if build dir is empty.
	if [ ! "$$(ls -A $(EXTERNAL_BUILD_DIR))" ]; then rm -rf build; fi
# Build from scratch if no build dir.
	if [ ! -d "build" ]; then make setup; fi
# Build.
	cd build; make

oss:
	if [ ! -d "build" ]; then make setup; fi
	cd build; cmake -DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE) ..
	cd build; make oss

clean:

cleanall: clean
	rm -rf build
	rm -rf $(EXTERNAL_BUILD_DIR)/*

.PHONY: default build do-build all clean cleanall
