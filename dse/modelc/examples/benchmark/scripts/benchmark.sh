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
#   $ sh dse/modelc/examples/benchmark/scripts/benchmark.sh 5 2000 40 1 1 1 1


# Input Parameters
# ================
: "${MODEL_COUNT:=$1}"
: "${SIGNAL_COUNT:=$2}"
: "${SIGNAL_CHANGE:=$3}"
: "${STACKED:=$4}"
: "${LOOPBACK:=$5}"
: "${STARTUP_IDX:=$6}"
: "${STARTUP_ANNO:=$7}"
: "${IMPORTER:=$8}"

: ${SIMBUS_TRANSPORT:=redis}
: ${SIMBUS_URI:=redis://localhost}
if [ ! -z $STACKED ] && [ $STACKED = "1" ]; then
    STACKED_OPT="-stacked"
fi
if [ ! -z $LOOPBACK ] && [ $LOOPBACK = "1" ]; then
    SIMBUS_TRANSPORT=loopback
    SIMBUS_URI=loopback
fi
if [ -z $STARTUP_IDX ]; then
    STARTUP_IDX=0
fi
if [ -z $STARTUP_ANNO ]; then
    STARTUP_ANNO=0
fi
if [ ! -z $IMPORTER ] && [ $IMPORTER = "fmi" ]; then
    SIMBUS_TRANSPORT=loopback
    SIMBUS_URI=loopback
else
    IMPORTER=simer
fi

# Envar Parameters
# ================
: "${MODEL_STEPSIZE:=0.0005}"
: "${MODEL_ENDTIME:=1.0}"
: "${MODEL_LOGGER:=4}"
: "${SIMER_IMAGE:=$DSE_SIMER_IMAGE}"
if [ -z $SIMER_IMAGE ]; then
    SIMER_IMAGE="simer:test"
fi
TOTAL_SIGNAL_CHANGE=$(( ${SIGNAL_CHANGE} * ${MODEL_COUNT} ))
STEP_COUNT=$(echo 'scale=0; 1.0/0.0005' | bc)
: "${TAG:=${SIMBUS_TRANSPORT}${STACKED_OPT}-${STARTUP_IDX}${STARTUP_ANNO}-${IMPORTER}_${SIGNAL_COUNT}_${TOTAL_SIGNAL_CHANGE}_${MODEL_COUNT}}"

simer()
{
     ( if test -d "$1"; then cd "$1" && shift; fi \
     && docker run -it --rm \
         -v $(pwd):/sim \
         -e SIMBUS_LOGLEVEL=${SIMBUS_LOGLEVEL:-$MODEL_LOGGER} \
         -e SIMBUS_TRANSPORT=${SIMBUS_TRANSPORT} \
         -e SIMBUS_URI=${SIMBUS_URI} \
         -e STARTUP_IDX=${STARTUP_IDX} \
         -e STARTUP_ANNO=${STARTUP_ANNO} \
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
    echo "STARTUP_IDX=${STARTUP_IDX}"
    echo "STARTUP_ANNO=${STARTUP_ANNO}"

    echo "STEP_COUNT=${STEP_COUNT}"
    echo "MODEL_STEPSIZE=${MODEL_STEPSIZE}"
    echo "MODEL_ENDTIME=${MODEL_ENDTIME}"
    echo "MODEL_LOGGER=${MODEL_LOGGER}"
    echo "SIMER_IMAGE=${SIMER_IMAGE}"
    echo "IMPORTER=${IMPORTER}"
}

benchmark()
{
    local models=$1
    local signals=$2
    local change=$3
    local stacked=$4

    extra/tools/benchmark/bin/benchmark signalgroup -count ${signals} -output dse/modelc/build/_out/examples/benchmark/data/signal_group.yaml
    extra/tools/benchmark/bin/benchmark simulation -count ${models} ${stacked} -change ${change} -output dse/modelc/build/_out/examples/benchmark/data/simulation.yaml
    if [ $IMPORTER = "fmi" ]; then
        rm -rf dse/modelc/build/_out/examples/fmu
        (cd dse/modelc/build/_out/examples/benchmark; task --taskfile ../../../../../../../dse.fmi/Taskfile.yml generate-fmimodelc SIM=. FMU_NAME=benchmark SIGNAL_GROUPS=data)
        ../dse.fmi/dse/build/_out/importer/fmuImporter --steps=${STEP_COUNT} dse/modelc/build/_out/examples/fmu/benchmark
    else
        simer dse/modelc/build/_out/examples/benchmark/ --endtime ${MODEL_ENDTIME}
    fi
}


conditions
export SIMBUS_LOGLEVEL=${SIMBUS_LOGLEVEL:-$MODEL_LOGGER}
export SIMBUS_TRANSPORT=${SIMBUS_TRANSPORT}
export SIMBUS_URI=${SIMBUS_URI}
export STARTUP_IDX=${STARTUP_IDX}
export STARTUP_ANNO=${STARTUP_ANNO}
export TAG=${TAG}
benchmark  ${MODEL_COUNT} ${SIGNAL_COUNT} ${SIGNAL_CHANGE} ${STACKED_OPT}
conditions
