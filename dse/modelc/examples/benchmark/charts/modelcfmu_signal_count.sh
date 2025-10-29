#!/bin/bash

# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

# Signal Count
# ============
#
# 2-D chart of increasing signal count over recommended topology set.


CHART_NAME=modelcfmu_signal_count
MODEL_COUNT=5
CHANGE_COUNT=200

rm -f dse/modelc/examples/benchmark/charts/${CHART_NAME}.txt

for DEPLOYMENT in simer fmu
do
    case $DEPLOYMENT in
        simer)
        LOOPBACK=1
        STACKED=1
        STARTUP_IDX=1
        STARTUP_ANNO=1
        IMPORTER=simer
        ;;
        fmu)
        LOOPBACK=1
        STACKED=1
        STARTUP_IDX=1
        STARTUP_ANNO=1
        IMPORTER=fmi
        ;;
    esac
    for SIGNAL_COUNT in 1000 2000 4000 8000 16000 32000
    do
        SIGNAL_CHANGE=$(( $CHANGE_COUNT / $MODEL_COUNT ))
        sh dse/modelc/examples/benchmark/scripts/benchmark.sh \
            $MODEL_COUNT \
            $SIGNAL_COUNT \
            $SIGNAL_CHANGE \
            $STACKED \
            $LOOPBACK \
            $STARTUP_IDX \
            $STARTUP_ANNO \
            $IMPORTER \
            2>&1 | tee -a dse/modelc/examples/benchmark/charts/${CHART_NAME}.txt | grep :::benchmark:
    done
done

extra/tools/benchmark/bin/benchmark chart \
    -title "Benchmark: Increasing Signal Count" \
    -conditions "200 signal change per step, 5 models, step size 0.5 mS (ThinkPad T16 G2, 1900 Mhz, 14 Core)" \
    -axis_index 1 \
    -axis_label "Total Signal Count" \
    -input dse/modelc/examples/benchmark/charts/${CHART_NAME}.txt
