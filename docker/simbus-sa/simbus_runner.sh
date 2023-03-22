#!/bin/bash

# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

# Input Parameters
# ==========
#   SIMBUS_YAML          - yaml configuration files
#                         as example SIMBUS_YAML="path/stack.yaml;path/foo.yaml;path/bar.yaml"
#                         (optional)
#
#           Follows parameters are ignored if set SIMBUS_YAML:
#
#   REDIS_URL           - URL contents host and port of the redis,
#                         as example REDIS_URL=redis://redis:6379
#                         (optional)
#   SIMBUS_CHANNELS     - List of channel names and expected model counts,
#                         as example SIMBUS_CHANNELS="test:1,raw:0,physical:0"
#                         (optional)
#   REDIS_TIMEOUT       - Redis timeout
#                         (optional)

: "${REDIS_TIMEOUT:=60}"
: "${SIMBUS_CHANNELS:=test:1}"
: "${SIMBUS_STEPSIZE:=0.005}"
: "${SIMBUS_LOGGER:=2}"
: "${SIMBUS_TIMEOUT:=3600}"
if [ -n "$DEBUG" ]; then
    echo "SimBus Runner"
    echo "============="
    echo ""
    echo "Environment:"
    echo "------------"
    echo "REDIS_URL=${REDIS_URL}"
    echo "REDIS_TIMEOUT=${REDIS_TIMEOUT}"
    echo "SIMBUS_CHANNELS=${SIMBUS_CHANNELS}"
    echo "SIMBUS_STEPSIZE=${SIMBUS_STEPSIZE}"
    echo "SIMBUS_YAML=${SIMBUS_YAML}"
    echo "SIMBUS_LOGGER=${SIMBUS_LOGGER}"
fi

: "${SIMBUS_EXE:=/usr/local/bin/simbus}"
: "${SIM_DIR:=/tmp/sim}"
mkdir -p $SIM_DIR

REDIS_HOST=redis
REDIS_PORT=6379


# SimBus Configuration
# ====================
# only create the YAML files not set
if [ ! -n "$SIMBUS_YAML" ]; then
    SIMBUS_YAML=/usr/local/etc/simbus.yaml
    mkdir -p $(dirname ${SIMBUS_YAML})

    # if set $REDIS_URL get connection REDIS_HOST and REDIS_PORT
    if [ -n "$REDIS_URL" ]; then
        REDIS_URL_arr=(${REDIS_URL//:\/\// })
        REDIS_host_port=${REDIS_URL_arr[-1]}
        IFS=: read -r REDIS_HOST REDIS_PORT <<< "$REDIS_host_port"
    fi

    SIMBUS_CHANNELS_JSON=""
    # Add channels based on $SIMBUS_CHANNELS
    if [ -n "$SIMBUS_CHANNELS" ]; then
        SIMBUS_CHANNELS_JSON=$(
        IFS=";" read -ra CHANNELS <<< "$SIMBUS_CHANNELS"
        for CHANNEL in "${CHANNELS[@]}"; do
            IFS=":" read -r name expectedModelCount <<< "$CHANNEL"
            jq -c -n \
            --arg name "$name" \
            --arg expectedModelCount "$expectedModelCount" \
            '{name: $name, expectedModelCount: $expectedModelCount}'
        done | jq -c -n '. |= [inputs]' )
    fi

cat > $SIMBUS_YAML << SIMBUSCONFIG
---
kind: Model
metadata:
  name: simbus
---
kind: Stack
metadata:
  name: SimBus Stack
spec:
  connection:
    transport:
      redispubsub:
        host: $REDIS_HOST
        port: $REDIS_PORT
        timeout: $REDIS_TIMEOUT
  models:
    - name: simbus
      model:
        name: simbus
      channels: $SIMBUS_CHANNELS_JSON
SIMBUSCONFIG
fi


# SimBus Command
# ==============
IFS=";" read -ra SIMBUS_YAML_ARR <<< "$SIMBUS_YAML"
SIMBUS_CMD="$SIMBUS_EXE --name simbus --stepsize ${SIMBUS_STEPSIZE} --logger $SIMBUS_LOGGER --timeout $SIMBUS_TIMEOUT ${SIMBUS_YAML_ARR[@]}"

if [ -n "$DEBUG" ]; then
    echo ""
    echo "SimBus Command"
    echo "--------------"
    echo "SIM_DIR=${SIM_DIR}"
    echo "SIMBUS_CMD=${SIMBUS_CMD}"
fi

while true; do
    echo ""
    echo "Start SimBus"
    echo "============"
    cd $SIM_DIR
    $SIMBUS_CMD
    status=$?
    if [ $status -ne 0 ] ; then
        echo SimBus Command exit code: $status
        exit $status
    fi
done
