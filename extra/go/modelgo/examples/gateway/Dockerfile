# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


# Gateway Example
#
# Usage:
#   cd sim
#   docker run -it --rm -v ./:/sim gateway:test

ARG SIMER_IMAGE=ghcr.io/boschglobal/dse-simer

# Gateway Builder
# ===============
FROM golang:bullseye AS gateway
ARG HTTPS_PROXY
ADD . /src
RUN cd /src/modelgo && make clean build


# Gateway Example Image
# =====================
FROM $SIMER_IMAGE
LABEL maintainer="timothy.rule@de.bosch.com"


# Install Gateway
# ===============
COPY --from=gateway /src/modelgo/bin/examples/gateway /usr/local/bin/gateway


# Setup the entry conditions
# ==========================
# Run docker with --volume parameter to map the simulation folder into /sim.
WORKDIR /sim
COPY modelgo/examples/gateway/entrypoint.sh /entrypoint_gateway.sh
RUN chmod +x /entrypoint_gateway.sh
ENTRYPOINT ["/entrypoint_gateway.sh"]
