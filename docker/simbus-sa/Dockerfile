# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

FROM debian:bullseye-slim
LABEL maintainer="timothy.rule@de.bosch.com"


# Container Runtime Environment Arguments
# =======================================
# Set the following to control the runtime operation of the container.
#   YAML_FILES      - yaml configuration files (optional)
#       -- Follows parameters are ignored if set YAML_FILES: --
#   REDIS_URL       - URL contents host and port of the Redis.
#   REDIS_TIMEOUT   - Redis timeout.
#   SIMBUS_CHANNELS - List of channel names and expected model counts.
#   SIMBUS_LOGGER   - Log level (lower == more, default 2).
#   SIMBUS_TIMEOUT  - Message timeout.
#
# Example:
#   docker network create dse
#   docker run --rm --name redis  --net dse -p 6379:6379 redis
#   docker run --rm -it --name simbus --net dse \
#               --env REDIS_URL="redis://redis:6379" \
#               --env SIMBUS_CHANNELS="test:1;raw:0;physical:0" \
#               dse-simbus-sa:latest
#   docker run --rm -it --name behave --net dse \
#               --volume $(pwd):/tmp/repo \
#               --env REDIS_URL="redis://redis:6379" \
#               --env MODULE_NAME=dse.behave.example.model \
#               dse-behave:latest


# Setup basic tools
# =================
RUN apt-get -y update; \
    apt-get -y upgrade; \
    apt-get -y install --no-install-recommends \
        bash \
        curl \
        less \
        jq \
        redis-tools \
        vim \
    && \
    apt-get clean; \
    rm -rf /var/lib/apt/lists/*


# Install the SimBus and Runner
# =============================
COPY dse/modelc/build/_out/bin/simbus /usr/local/bin/simbus
COPY docker/simbus-sa/simbus_runner.sh /simbus_runner.sh
RUN chmod +x /simbus_runner.sh


# Setup the entry conditions
# ==========================
WORKDIR /
CMD ["/bin/bash", "-c", "/simbus_runner.sh"]
