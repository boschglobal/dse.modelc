# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


SRC           := $(shell find . -type f -name '*.go' -print)
BIN_DIR       := $(CURDIR)/bin
BUILD_DIR     := $(CURDIR)/build/stage
CLI_TOOLS     ?= $(BIN_DIR)/trace
CLI_TOOLS_WIN := $(addsuffix .exe,$(CLI_TOOLS))
GOFLAGS       :=
GOPRIVATE     ?= github.com/boschglobal


.PHONY: all
all: build


.PHONY: build
build: cmd

cmd: $(CLI_TOOLS)

$(CLI_TOOLS): $(SRC)
	go build $(GOFLAGS) -o $@ ./cmd/$(subst $(BIN_DIR)/,,$@)

run:
#	bin/trace --verbose convert --csv testdata/simbus.bin
#	bin/trace --verbose convert --asc testdata/simbus_pdu.bin
	bin/trace --verbose convert --asc --name foo --tx 5 testdata/ncodec_pdu.bin

.PHONY: win
win: $(CLI_TOOLS_WIN)

$(CLI_TOOLS_WIN): $(SRC)
	GOOS=windows GOARCH=amd64 go build $(GOFLAGS) -o $@ ./cmd/$(basename $(subst $(BIN_DIR)/,,$@))

.PHONY: test
test:
	go test ./...
	go test ./... -tags e2e


.PHONY: lint
lint:
	golangci-lint fmt ./...
	golangci-lint run ./... --fix


.PHONY: clean
clean:
	@rm -rf '$(BIN_DIR)'
