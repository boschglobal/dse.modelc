#!/bin/bash

# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

# Model Fanout
# ============
#
# 2-D chart of constant signal throughput for increasing model count over
# recommended topology set. Overall signal exchange per simulation step
# remains constant


CHART_NAME=model_fanout
SIGNAL_COUNT=4000
CHANGE_COUNT=400

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
    for MODEL_COUNT in 1 2 3 4 6 8 12 16
    do
        SIGNAL_CHANGE=$(( $CHANGE_COUNT / $MODEL_COUNT ))
        sh dse/modelc/examples/benchmark/scripts/benchmark.sh \
            $MODEL_COUNT \
            $SIGNAL_COUNT \
            $SIGNAL_CHANGE \
            $STACKED \
            $LOOPBACK \
            2>&1 | tee -a dse/modelc/examples/benchmark/charts/${CHART_NAME}.txt | grep :::benchmark:
    done
done

extra/tools/benchmark/bin/benchmark chart \
    -title "Benchmark: Increasing Model Count" \
    -conditions "CoSim w. step 0.5 mS (ThinkPad T16 G2, 1900 Mhz, 14 Core)" \
    -input dse/modelc/examples/benchmark/charts/${CHART_NAME}.txt
