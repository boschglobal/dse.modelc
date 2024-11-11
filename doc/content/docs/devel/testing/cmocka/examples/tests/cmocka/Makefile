
GDB_CMD ?= valgrind -q --leak-check=yes
# GDB_CMD ?= gdb -q -ex='set confirm on' -ex=run -ex=quit

default: build

setup:
	mkdir build;
	cd build; cmake ..

build:
# Build from scratch if no build dir.
	if [ ! -d "build" ]; then make setup; fi
# Build.
	cd build; make
	cd build; make install

.PHONY: run
run:
	cd build/_out; $(GDB_CMD) bin/test_target

.PHONY: clean
clean:
	rm -rf build

.PHONY: cleanall
cleanall: clean
