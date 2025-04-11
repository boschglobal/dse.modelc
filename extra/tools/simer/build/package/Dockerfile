# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


# SIMER Simulation Runtime
#
# Usage:
#	simer() { ( cd "$1" && shift && docker run -it --rm -v $(pwd):/sim simer:test "$@"; ) }
#	(then)
#	simer dse/modelc/build/_out/examples/minimal -endtime 0.002


# Redis Builder
# =============
FROM debian:bullseye-slim AS redis

ENV REDIS_VERSION=6.0.20
ENV REDIS_DOWNLOAD_URL=http://download.redis.io/releases/redis-6.0.20.tar.gz
ENV REDIS_DOWNLOAD_SHA=173d4c5f44b5d7186da96c4adc5cb20e8018b50ec3a8dfe0d191dbbab53952f0


RUN set -eux; \
	groupadd -r -g 999 redis; \
	useradd -r -g redis -u 999 redis

RUN set -eux; \
	apt-get update; \
	apt-get install -y --no-install-recommends \
		ca-certificates \
		wget \
		dpkg-dev \
		gcc \
		libc6-dev \
		libssl-dev \
		make \
	; \
	rm -rf /var/lib/apt/lists/*; \
	\
	wget -O redis.tar.gz "$REDIS_DOWNLOAD_URL"; \
	echo "$REDIS_DOWNLOAD_SHA *redis.tar.gz" | sha256sum -c -; \
	mkdir -p /usr/src/redis; \
	tar -xzf redis.tar.gz -C /usr/src/redis --strip-components=1; \
	rm redis.tar.gz; \
	\
    # Disable Redis protected mode.
	grep -E '^ *createBoolConfig[(]"protected-mode",.*, *1 *,.*[)],$' /usr/src/redis/src/config.c; \
	sed -ri 's!^( *createBoolConfig[(]"protected-mode",.*, *)1( *,.*[)],)$!\10\2!' /usr/src/redis/src/config.c; \
	grep -E '^ *createBoolConfig[(]"protected-mode",.*, *0 *,.*[)],$' /usr/src/redis/src/config.c; \
	\
	make -C /usr/src/redis -j "$(nproc)" all; \
	make -C /usr/src/redis install; \
	rm -r /usr/src/redis;


# Simer Builder
# =============
FROM golang:bullseye AS simer
ADD . /src
RUN cd /src && make build


# Trace Builder
# =============
FROM golang:bullseye AS trace
ADD build/stage/trace /src
RUN cd /src && make build


# Simer Container Image
# =====================
FROM debian:bullseye-slim
LABEL maintainer="timothy.rule@de.bosch.com"

ENV DEBIAN_FRONTEND=noninteractive

RUN set -eux; \
	groupadd -r -g 999 redis; \
	useradd -r -g redis -u 999 redis

RUN set -eux; \
    dpkg --add-architecture i386; \
	apt-get -y update; \
	apt-get -y upgrade; \
    apt-get -y install --no-install-recommends \
        bash \
        ca-certificates \
        curl \
		gdb \
        gdbserver \
        jq \
        less \
		libc6-i386 \
        libc6-x32 \
        libx32stdc++6 \
		tmux \
		valgrind \
        vim \
        unzip \
    && \
    apt-get clean; \
    rm -rf /var/lib/apt/lists/*;


# Install Redis
# =============
COPY build/package/redis.conf /usr/local/etc/redis/redis.conf
COPY --from=redis /usr/local/bin/redis-cli /usr/local/bin/redis-cli
COPY --from=redis /usr/local/bin/redis-server /usr/local/bin/redis-server
RUN mkdir -p /data/redis \
	&& chown redis:redis /data/redis \
	&& mkdir -p /var/run/redis \
	&& chown redis:redis /var/run/redis
VOLUME /data/redis
EXPOSE 6379


# Setup GDB Server
# ================
EXPOSE 2159


# Install ModelC Tools and  MCL Model
# ===================================
ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib32:/usr/local/libx32
COPY build/stage/bin/* /usr/local/bin/
COPY build/stage/lib/* /usr/local/lib/
COPY build/stage/lib32/* /usr/local/lib32/
COPY build/stage/libx32/* /usr/local/libx32/


# Install Simer
# =============
COPY --from=simer /src/bin/simer /usr/local/bin/simer


# Install Trace
# =============
COPY --from=trace /src/bin/trace /usr/local/bin/trace


# Add License Information (from 3rd party repos)
# =======================
COPY build/stage/licenses /licenses


# Setup the entry conditions
# ==========================
# Run docker with --volume parameter to map the simulation folder into /sim.
WORKDIR /sim
COPY build/package/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
