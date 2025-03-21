#!/bin/bash

# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

# Signal Throughput
# =================
#
# 2-D chart of increasing signal throughput over recommended topology set.


CHART_NAME=signal_throughput
MODEL_COUNT=5
SIGNAL_COUNT=4000

rm -f dse/modelc/examples/benchmark/charts/${CHART_NAME}.txt

for TOPOLOGY in runtime redis_stacked redis_distributed
do
    case $TOPOLOGY in
        runtime)
        LOOPBACK=1
        STACKED=1
        ;;
        redis_stacked)
        LOOPBACK=0
        STACKED=1
        ;;
        redis_distributed)
        LOOPBACK=0
        STACKED=0
        ;;
    esac
    for SIGNAL_CHANGE in 25 50 100 200 400 800
    do
        sh dse/modelc/examples/benchmark/scripts/benchmark.sh \
            $MODEL_COUNT \
            $SIGNAL_COUNT \
            $SIGNAL_CHANGE \
            $STACKED \
            $LOOPBACK \
            2>&1 | tee -a dse/modelc/examples/benchmark/charts/${CHART_NAME}.txt | grep :::benchmark:
    done
done
