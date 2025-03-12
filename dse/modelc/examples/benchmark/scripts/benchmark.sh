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
#
# Run
# ---
#   $ sh dse/modelc/examples/benchmark/benchmark.sh 5 200
#   $ sh dse/modelc/examples/benchmark/benchmark.sh 5 200 1 1


# Input Parameters
# ================
: "${MODEL_COUNT:=$1}"
: "${SIGNAL_COUNT:=$2}"
: "${STACKED:=$3}"
: "${LOOPBACK:=$4}"

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
: "${MODEL_LOGGER:=4}"


simer()
{
     ( if test -d "$1"; then cd "$1" && shift; fi \
     && docker run -it --rm \
         -v $(pwd):/sim \
         -e SIMBUS_LOGLEVEL=${SIMBUS_LOGLEVEL:-4} \
         -e SIMBUS_TRANSPORT=${SIMBUS_TRANSPORT:-redis} \
         -e SIMBUS_URI=${SIMBUS_URI:-redis://localhost} \
         -p 2159:2159 \
         -p 6379:6379 \
         $DSE_SIMER_IMAGE "$@"; ); \
}

conditions()
{
    echo "Benchmark"
    echo "========="
    echo "MODEL_COUNT=${MODEL_COUNT}"
    echo "SIGNAL_COUNT=${SIGNAL_COUNT}"
    echo "STACKED=${STACKED} (opt: ${STACKED_OPT})"
    echo "LOOPBACK=${LOOPBACK}"

    echo "MODEL_STEPSIZE=${MODEL_STEPSIZE}"
    echo "MODEL_ENDTIME=${MODEL_ENDTIME}"
    echo "MODEL_LOGGER=${MODEL_LOGGER}"
}

benchmark()
{
    local models=$1
    local signals=$2
    local stacked=$3

    extra/tools/benchmark/bin/benchmark signalgroup -count ${signals} -output dse/modelc/build/_out/examples/benchmark/data/signal_group.yaml
    extra/tools/benchmark/bin/benchmark simulation -count ${models} ${stacked} -output dse/modelc/build/_out/examples/benchmark/data/simulation.yaml
    simer dse/modelc/build/_out/examples/benchmark/ --endtime ${MODEL_ENDTIME}
}


conditions
benchmark  ${MODEL_COUNT} ${SIGNAL_COUNT} ${STACKED_OPT}
conditions
