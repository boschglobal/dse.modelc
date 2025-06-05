# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


SRC           := $(shell find . -type f -name '*.go' -print)
GOFLAGS       :=
GOPRIVATE     ?= github.com/boschglobal


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
