# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


PACKAGE  		:= $(shell basename $(shell pwd))
SRC           	:= $(shell find . -type f -name '*.go' -print)
BIN_DIR       	:= $(CURDIR)/bin
CLI_TOOLS    	?= $(BIN_DIR)/$(PACKAGE)
GOFLAGS       	:=
GOPRIVATE     	?= github.com/boschglobal,github.boschdevcloud.com

.PHONY: all
all: build

.PHONY: build
build:

.PHONY: test
test:
	go test ./...
