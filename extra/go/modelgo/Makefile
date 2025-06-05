# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


SRC           		:= $(shell find . -type f -name '*.go' -print)
BIN_DIR       		:= $(CURDIR)/bin
GOFLAGS       		:=
GOPRIVATE     		?= github.com/boschglobal
EXAMPLES_BIN_DIR	?= $(CURDIR)/bin/examples
EXAMPLES     		?= $(EXAMPLES_BIN_DIR)/gateway
SIMER_BASE			:= $(if $(SIMER),"--build-arg=SIMER_IMAGE=$(SIMER)",)

.PHONY: all
all: build


.PHONY: build
build: examples


examples: $(EXAMPLES)

$(EXAMPLES): $(SRC)
	go build $(GOFLAGS) -o $@ ./examples/$(subst $(EXAMPLES_BIN_DIR)/,,$@)


.PHONY: test
test:
	go test ./...

.PHONY: test_e2e
test_e2e: docker
	go test ./examples/... -tags test_e2e -v


.PHONY: docker
docker:
	docker build -f examples/gateway/Dockerfile $(SIMER_BASE) --tag gateway:test .. ;\


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
