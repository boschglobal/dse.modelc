# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

FROM debian:bookworm-slim
LABEL maintainer="timothy.rule@de.bosch.com"


# Container Runtime Environment Arguments
# =======================================
# Set the following to control the runtime operation of the container.
#   REDIS_URL           - URL contents host and port of the Redis.
#   REDIS_TIMEOUT       - Redis timeout.
#   YAML_FILES          - List of configuration YAML files separated semicolon.
#   MODEL_INSTANCE      - Model instance name.
#   MODEL_LOGGER        - Log level (lower == more, default 2).
#   MODEL_TIMEOUT       - Message timeout (default 60 seconds).
#
# Example:
#   docker network create dse
#   docker run --rm --name redis  --net dse -p 6379:6379 redis
#   docker run --rm -it --name simbus --net dse \
#               --volume $(pwd):/tmp/repo \
#               --env YAML_FILES="/tmp/repo/stack.yaml" \
#               dse-simbus-sa:latest
#
#   docker run --rm -it --name model --net dse \
#               --volume $(pwd):/tmp/repo \
#               --env MODEL_INSTANCE=model_instance \
#               --env YAML_FILES="/tmp/repo/stack.yaml;/tmp/repo/foo.yaml;/tmp/repo/bar.yaml" \
#               dse-modelc:latest


# Setup basic tools
# =================
RUN apt-get -y update; \
    apt-get -y upgrade; \
    apt-get -y install --no-install-recommends \
        bash \
        ca-certificates \
        curl \
        gdb \
        jq \
        less \
        redis-tools \
        vim \
        unzip \
    && \
    apt-get clean; \
    rm -rf /var/lib/apt/lists/*


# Install the ModelC, MCL Model, and Runner
# =========================================
COPY dse/modelc/build/_out/bin/modelc /usr/local/bin/modelc
COPY dse/modelc/build/_out/lib/libmodelc.so /usr/local/bin/libmodelc.so
COPY dse/modelc/build/_out/lib/mcl_model.so /usr/local/lib/mcl_model.so
COPY extra/docker/modelc/modelc_runner.sh /modelc_runner.sh
RUN chmod +x /modelc_runner.sh


# Setup the entry conditions
# ==========================
WORKDIR /
CMD ["/bin/bash", "-c", "/modelc_runner.sh"]
