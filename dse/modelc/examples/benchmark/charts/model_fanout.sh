#!/bin/bash

# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

# Model Fanout
# ============
#
# 2-D chart of constant signal throughput for increasing model count over
# recommended topology set.


CHART_NAME=model_fanout
SIGNAL_COUNT=8000

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
    for MODEL_COUNT in 1 2 3 4 5 6 7 8 9 10
    do
        SIGNAL_CHANGE=$(( $SIGNAL_COUNT / $MODEL_COUNT / 10 ))
        sh dse/modelc/examples/benchmark/scripts/benchmark.sh \
            $MODEL_COUNT \
            $SIGNAL_COUNT \
            $SIGNAL_CHANGE \
            $STACKED \
            $LOOPBACK \
            2>&1 | tee -a dse/modelc/examples/benchmark/charts/${CHART_NAME}.txt | grep ::benchmark:
    done
done
