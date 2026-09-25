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

for TOPOLOGY in runtime redis_stacked redis_distributed unix_stream_distributed tcp_stream_distributed
do
    case $TOPOLOGY in
        runtime)
        LOOPBACK=1
        STACKED=1
        STREAM=0
        TCP=0
        ;;
        redis_stacked)
        LOOPBACK=0
        STACKED=1
        STREAM=0
        TCP=0
        ;;
        redis_distributed)
        LOOPBACK=0
        STACKED=0
        STREAM=0
        TCP=0
        ;;
        unix_stream_distributed)
        LOOPBACK=0
        STACKED=0
        STREAM=1
        TCP=0
        ;;
        tcp_stream_distributed)
        LOOPBACK=0
        STACKED=0
        STREAM=1
        TCP=1
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
            $STREAM \
            $TCP \
            2>&1 | tee -a dse/modelc/examples/benchmark/charts/${CHART_NAME}.txt | grep :::benchmark:
    done
done

extra/tools/benchmark/bin/benchmark chart \
    -title "Benchmark: Increasing Signal Throughput" \
    -conditions "4000 signals, 5 models, step size 0.5 mS (ThinkPad T16 G2, 1900 Mhz, 14 Core)" \
    -axis_index 2 \
    -axis_label "Signal Throughput" \
    -input dse/modelc/examples/benchmark/charts/${CHART_NAME}.txt
