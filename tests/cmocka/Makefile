# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

export MAKE_NPROC ?= $(shell nproc)
export LD_LIBRARY_PATH=$(shell pwd)/build/_out/lib

# 41 = ASCII 'A'/65
# 42 = ASCII 'B'/66
ifeq ($(GDB_CMD),)
GDB_CMD = valgrind -q --leak-check=yes --free-fill=42 --malloc-fill=41
endif
# GDB_CMD = LD_DEBUG=libs
# GDB_CMD = gdb -q -ex='set confirm on' -ex=run -ex=quit

default: build

setup:
	mkdir build;
	cd build; cmake -DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE) ..

build:
# Build from scratch if no build dir.
	if [ ! -d "build" ]; then make setup; fi
# Build.
	cd build; cmake --build . -j $(MAKE_NPROC)
	cd build; cmake --build . -j $(MAKE_NPROC) -t install
	@echo ""
	@echo "Sandbox files: - $$(pwd)/build/_out"
	@echo "--------------"
	@find build/_out/ -type f -name '*' -exec ls -sh --color=auto {} \;

run:
	@echo "[ GDB_CMD  ] $(GDB_CMD)"
	@echo "[----------]"
	@cd build/_out; $(GDB_CMD) bin/test_model
	@cd build/_out; $(GDB_CMD) bin/test_model_interface
	@cd build/_out; $(GDB_CMD) bin/test_simbus_loopback
	@echo "[----------]"
	@echo "[ GDB_CMD  ] $(GDB_CMD)"

clean:
	rm -rf build

cleanall: clean

.PHONY: default build run all clean cleanall
