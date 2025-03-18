#!/bin/bash

# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


# Operation
# =========
#
# Setup
# -----
#   $ make
#   $ make simer tools
#   $ sudo /etc/init.d/redis-server stop
#
# Run
# ---
#   $ sh dse/modelc/examples/benchmark/scripts/benchmark.sh 5 2000 40
#   $ sh dse/modelc/examples/benchmark/scripts/benchmark.sh 5 2000 40 1 1


# Input Parameters
# ================
: "${MODEL_COUNT:=$1}"
: "${SIGNAL_COUNT:=$2}"
: "${SIGNAL_CHANGE:=$3}"
: "${STACKED:=$4}"
: "${LOOPBACK:=$5}"

: ${SIMBUS_TRANSPORT:=redis}
: ${SIMBUS_URI:=redis://localhost}
if [ ! -z $STACKED ] && [ $STACKED = "1" ]; then
    STACKED_OPT="-stacked"
fi
if [ ! -z $LOOPBACK ] && [ $LOOPBACK = "1" ]; then
    SIMBUS_TRANSPORT=loopback
    SIMBUS_URI=loopback
fi

# Envar Parameters
# ================
: "${MODEL_STEPSIZE:=0.0005}"
: "${MODEL_ENDTIME:=1.0}"
: "${MODEL_LOGGER:=5}"
: "${SIMER_IMAGE:=$DSE_SIMER_IMAGE}"
if [ -z $SIMER_IMAGE ]; then
    SIMER_IMAGE="simer:test"
fi
TOTAL_SIGNAL_CHANGE=$(( ${SIGNAL_CHANGE} * ${MODEL_COUNT} ))
: "${TAG:=${SIMBUS_TRANSPORT}${STACKED_OPT}-${SIGNAL_COUNT}-${TOTAL_SIGNAL_CHANGE}-${MODEL_COUNT}}"

simer()
{
     ( if test -d "$1"; then cd "$1" && shift; fi \
     && docker run -it --rm \
         -v $(pwd):/sim \
         -e SIMBUS_LOGLEVEL=${SIMBUS_LOGLEVEL:-$MODEL_LOGGER} \
         -e SIMBUS_TRANSPORT=${SIMBUS_TRANSPORT} \
         -e SIMBUS_URI=${SIMBUS_URI} \
         -e TAG=${TAG} \
         -p 2159:2159 \
         -p 6379:6379 \
         $SIMER_IMAGE "$@"; ); \
}

conditions()
{
    echo "Benchmark"
    echo "========="
    echo "MODEL_COUNT=${MODEL_COUNT}"
    echo "SIGNAL_COUNT=${SIGNAL_COUNT}"
    echo "SIGNAL_CHANGE=${SIGNAL_CHANGE}"
    echo "STACKED=${STACKED} (opt: ${STACKED_OPT})"
    echo "LOOPBACK=${LOOPBACK}"

    echo "MODEL_STEPSIZE=${MODEL_STEPSIZE}"
    echo "MODEL_ENDTIME=${MODEL_ENDTIME}"
    echo "MODEL_LOGGER=${MODEL_LOGGER}"
    echo "SIMER_IMAGE=${SIMER_IMAGE}"
}

benchmark()
{
    local models=$1
    local signals=$2
    local change=$3
    local stacked=$4

    extra/tools/benchmark/bin/benchmark signalgroup -count ${signals} -output dse/modelc/build/_out/examples/benchmark/data/signal_group.yaml
    extra/tools/benchmark/bin/benchmark simulation -count ${models} ${stacked} -change ${change} -output dse/modelc/build/_out/examples/benchmark/data/simulation.yaml
    simer dse/modelc/build/_out/examples/benchmark/ --endtime ${MODEL_ENDTIME}
}


conditions
export SIMBUS_TRANSPORT=${SIMBUS_TRANSPORT}
export SIMBUS_URI=${SIMBUS_URI}
export TAG=${TAG}
benchmark  ${MODEL_COUNT} ${SIGNAL_COUNT} ${SIGNAL_CHANGE} ${STACKED_OPT}
conditions
