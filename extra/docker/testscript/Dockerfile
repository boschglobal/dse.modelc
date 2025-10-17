# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


# Testscript Runtime
#
# Usage (supporting docker in docker):
#
#	docker run -it --rm \
#       -e ENTRYDIR=$(pwd) \
#       -v /var/run/docker.sock:/var/run/docker.sock \
#       -v $(pwd):/repo \
#       $(TESTSCRIPT_IMAGE) \
#           -e ENTRYDIR=$(pwd) \
#           tests/testscript/e2e/testcase.txtar


# Go Builder
# ==========
FROM golang:bookworm as go
RUN GOBIN=/usr/local/bin go install github.com/rogpeppe/go-internal/cmd/testscript@latest
RUN GOBIN=/usr/local/bin go install github.com/go-task/task/v3/cmd/task@latest



# Testscript Container Image
# ==========================
FROM debian:bookworm-slim
LABEL maintainer="timothy.rule@de.bosch.com"

RUN apt-get -y update; \
    apt-get -y upgrade; \
    apt-get -y install --no-install-recommends \
        ca-certificates \
        curl \
        git \
        gnupg \
        jq \
        less \
        lsb-release \
        make \
        unzip \
        vim \
        zip \
    && \
    apt-get clean; \
    rm -rf /var/lib/apt/lists/*

# Install docker.
RUN curl -fsSL https://download.docker.com/linux/ubuntu/gpg | gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg \
    && \
    echo\
        "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] \
        https://download.docker.com/linux/debian \
        $(lsb_release -cs) stable" \
        | tee /etc/apt/sources.list.d/docker.list > /dev/null

# Install yq using curl.
RUN curl -L https://github.com/mikefarah/yq/releases/download/v4.20.1/yq_linux_amd64 -o /usr/local/bin/yq \
    && chmod +x /usr/local/bin/yq

RUN set -eux; \
	apt-get -y update; \
	apt-get -y upgrade; \
    apt-get -y install --no-install-recommends \
        docker-ce-cli \
    && \
    apt-get clean; \
    rm -rf /var/lib/apt/lists/*;

# Complete image.
COPY --from=go /usr/local/bin/testscript /usr/local/bin/testscript
COPY --from=go /usr/local/bin/task /usr/local/bin/task

WORKDIR /repo
ENTRYPOINT ["/usr/local/bin/testscript"]
