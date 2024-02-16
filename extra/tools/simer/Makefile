# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


SRC           := $(shell find . -type f -name '*.go' -print)
BIN_DIR       := $(CURDIR)/bin
BUID_DIR      := $(CURDIR)/build/stage
CLI_TOOLS     ?= $(BIN_DIR)/simer
GOFLAGS       :=
GOPRIVATE     ?= github.com/boschglobal


.PHONY: all
all: build

.PHONY: run_noredis
run_noredis: build
	cd ../../../dse/modelc/build/_out/examples/simer; \
		$(BIN_DIR)/simer \
		-redis="" \
		-simbus=$(BUID_DIR)/bin/simbus \
		-modelc=$(BUID_DIR)/bin/modelc \
		-modelcX32=$(BUID_DIR)/bin/modelc32 \
		-logger=2


.PHONY: build
build: cmd

cmd: $(CLI_TOOLS)

$(CLI_TOOLS): $(SRC)
	go build $(GOFLAGS) -o $@ ./cmd/$(subst $(BIN_DIR)/,,$@)


.PHONY: test
test:
	go test ./...
	go test ./... -tags e2e


.PHONY: lint
lint:
	go fmt -x ./...
	PATH=~/go/bin:$$PATH; goimports -w -v .
	docker run -t --rm \
		-v $$(pwd):/app \
		-v ~/.cache/golangci-lint/v1.54.2:/root/.cache \
		-w /app \
		golangci/golangci-lint:latest \
		golangci-lint run -v ./...


.PHONY: clean
clean:
	@rm -rf '$(BIN_DIR)'
