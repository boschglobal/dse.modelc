#!/bin/bash

# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

# Signal Count
# ============
#
# 2-D chart of increasing signal count over recommended topology set.


CHART_NAME=startup_speed
MODEL_COUNT=5
CHANGE_COUNT=200

rm -f dse/modelc/examples/benchmark/charts/${CHART_NAME}.txt

for TOPOLOGY in runtime runtime_idx runtime_anno runtime_idx_anno
do
    case $TOPOLOGY in
        runtime)
        LOOPBACK=1
        STACKED=1
        STARTUP_IDX=0
        STARTUP_ANNO=0
        ;;
        runtime_idx)
        LOOPBACK=1
        STACKED=1
        STARTUP_IDX=1
        STARTUP_ANNO=0
        ;;
        runtime_anno)
        LOOPBACK=1
        STACKED=1
        STARTUP_IDX=0
        STARTUP_ANNO=1
        ;;
        runtime_idx_anno)
        LOOPBACK=1
        STACKED=1
        STARTUP_IDX=1
        STARTUP_ANNO=1
        ;;
    esac
    for SIGNAL_COUNT in 1000 2000 4000 8000 16000 32000
    do
        sh dse/modelc/examples/benchmark/scripts/benchmark.sh \
            $MODEL_COUNT \
            $SIGNAL_COUNT \
            $CHANGE_COUNT \
            $STACKED \
            $LOOPBACK \
            $STARTUP_IDX \
            $STARTUP_ANNO \
            2>&1 | tee -a dse/modelc/examples/benchmark/charts/${CHART_NAME}.txt | grep :::benchmark:
    done
done

extra/tools/benchmark/bin/benchmark chart \
    -title "Benchmark: Startup Speed" \
    -conditions "5 models, 200 signal change/step, step 0.5mS, loopback/stacked (ThinkPad T16 G2, 1900 Mhz, 14 Core)" \
    -axis_index 1 \
    -axis_label "Total Signal Count" \
    -sample_index 8 \
    -input dse/modelc/examples/benchmark/charts/${CHART_NAME}.txt
