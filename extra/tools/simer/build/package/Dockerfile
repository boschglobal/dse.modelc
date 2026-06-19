# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


# SIMER Simulation Runtime
#
# Usage:
#	simer() { ( cd "$1" && shift && docker run -it --rm -v $(pwd):/sim simer:test "$@"; ) }
#	(then)
#	simer dse/modelc/build/_out/examples/minimal -endtime 0.002


# Valkey Builder
# ==============
FROM debian:bookworm-slim AS valkey

ENV VALKEY_VERSION=8.0.1
ENV VALKEY_DOWNLOAD_URL=https://github.com/valkey-io/valkey/archive/refs/tags/8.0.1.tar.gz

RUN set -eux; \
	groupadd -g 1000 simer; \
	useradd -u 1000 -g simer -m -s /bin/bash simer

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
	wget -O valkey.tar.gz "$VALKEY_DOWNLOAD_URL"; \
	mkdir -p /usr/src/valkey; \
	tar -xzf valkey.tar.gz -C /usr/src/valkey --strip-components=1; \
	rm valkey.tar.gz; \
	\
	make -C /usr/src/valkey -j "$(nproc)" all; \
	make -C /usr/src/valkey install; \
	rm -r /usr/src/valkey;


# Simer Builder
# =============
FROM golang:bookworm AS simer
ADD . /src
RUN cd /src && make build


# Trace Builder
# =============
FROM golang:bookworm AS trace
ADD build/stage/trace /src
RUN cd /src && make clean && make build


# Simer Container Image
# =====================
FROM debian:bookworm-slim AS simer-slim
LABEL maintainer="timothy.rule@de.bosch.com"

ENV DEBIAN_FRONTEND=noninteractive

RUN set -eux; \
	groupadd -g 1000 simer; \
	useradd -u 1000 -g simer -m -s /bin/bash simer;

RUN set -eux; \
    dpkg --add-architecture i386; \
	apt-get -y update; \
	apt-get -y upgrade; \
    apt-get -y install --no-install-recommends \
        bash \
        ca-certificates \
        curl \
		gosu \
        jq \
		libc6-i386 \
        libc6-x32 \
        libx32stdc++6 \
        unzip \
    && \
    apt-get clean; \
	rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* /usr/share/doc /usr/share/man;



# Install Valkey
# ==============
COPY build/package/valkey.conf /usr/local/etc/valkey/valkey.conf
COPY --from=valkey /usr/local/bin/valkey-cli /usr/local/bin/valkey-cli
COPY --from=valkey /usr/local/bin/valkey-server /usr/local/bin/valkey-server
RUN mkdir -p /data/valkey \
	&& chown simer:simer /data/valkey \
	&& mkdir -p /var/run/valkey \
	&& chown simer:simer /var/run/valkey
VOLUME /data/valkey
EXPOSE 6379


# Install ModelC Tools and  MCL Model
# ===================================
ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib32:/usr/local/libx32
COPY build/stage/bin/ /usr/local/bin/
COPY build/stage/lib/ /usr/local/lib/
COPY build/stage/share/ /usr/local/share/
COPY build/stage/lib32/ /usr/local/lib32/
COPY build/stage/libx32/ /usr/local/libx32/


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

# FIXME this change does not work in E2E tests.
#
#RUN printf 'precedence ::ffff:0:0/96  100\n' >> /etc/gai.conf

FROM simer-slim

RUN set -eux; \
	apt-get -y update; \
    apt-get -y install --no-install-recommends \
		gdb \
        gdbserver \
        less \
		tmux \
		valgrind \
        vim \
    && \
    apt-get clean; \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* /usr/share/doc /usr/share/man;

# Setup GDB Server
# ================
EXPOSE 2159
