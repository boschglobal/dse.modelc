# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


# DSE Inspect
#
# Usage:
#	docker run --rm -v $(pwd):/sim inspect:test -all-files


# Inspect Builder
# ===============
FROM golang:1.23-alpine AS inspect
WORKDIR /src
COPY go.mod go.sum ./
RUN --mount=type=cache,target=/go/pkg/mod \
    go mod download
COPY cmd/inspect cmd/inspect
COPY internal internal
RUN --mount=type=cache,target=/root/.cache/go-build \
    --mount=type=cache,target=/go/pkg/mod \
    CGO_ENABLED=0 GOOS=linux go build -ldflags="-w -s" -o /inspect ./cmd/inspect


# DSE Inspect Image
# =================
FROM gcr.io/distroless/static-debian12:nonroot
WORKDIR /sim
COPY --from=inspect /inspect /usr/local/bin/inspect
USER nonroot:nonroot
ENTRYPOINT ["/usr/local/bin/inspect"]
