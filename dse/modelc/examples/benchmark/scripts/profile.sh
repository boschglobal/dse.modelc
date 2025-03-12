#!/bin/bash

# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


# Operation
# =========
#
# Simulation is run stacked in a single ModelC instance.
#
# Setup
# -----
#   $ PROFILEFLAGS=1 make build simer tools
#   $
#   $ sudo /etc/init.d/redis-server start
#
# Run
# ---
#   $ sh dse/modelc/examples/benchmark/profile.sh
#   $ gprof -p dse/modelc/build/_out/bin/simbus  dse/modelc/build/_out/examples/benchmark/simbus/gmon.out
#   $ gprof -p dse/modelc/build/_out/bin/modelc  dse/modelc/build/_out/examples/benchmark/gmon.out


# Parameters
# ==========
: "${SIM_DIR:=dse/modelc/build/_out/examples/benchmark}"
: "${MODEL_COUNT:=5}"
: "${SIGNAL_COUNT:=2000}"
: "${MODEL_STEPSIZE:=0.0005}"
: "${MODEL_ENDTIME:=1.0}"

: "${SIMBUS_EXE:=../../../bin/simbus}"
: "${MODELC_EXE:=../../bin/modelc}"
: "${SIMBUS_LOGGER:=4}"
: "${MODELC_LOGGER:=4}"

: "${SIMBUS_TRANSPORT=redis}
: "${SIMBUS_URI=redis://localhost}

MODEL_NAMES="benchmark_inst_1;benchmark_inst_2;benchmark_inst_3;benchmark_inst_4;benchmark_inst_5"
YAML_FILES="data/simulation.yaml data/signal_group.yaml data/model.yaml"
SIMBUS_CMD="$SIMBUS_EXE --name simbus --stepsize ${MODEL_STEPSIZE} --logger ${SIMBUS_LOGGER} ../data/simulation.yaml"
MODELC_CMD="$MODELC_EXE --stepsize ${MODEL_STEPSIZE} --endtime ${MODEL_ENDTIME} --name ${MODEL_NAMES} --logger $MODELC_LOGGER ${YAML_FILES}"


generate()
{
    extra/tools/benchmark/bin/benchmark signalgroup -count ${SIGNAL_COUNT} -output dse/modelc/build/_out/examples/benchmark/data/signal_group.yaml
    extra/tools/benchmark/bin/benchmark simulation -count ${MODEL_COUNT} -stacked -output dse/modelc/build/_out/examples/benchmark/data/simulation.yaml
}

profile()
{
    # Run the SimBus in the sim/lib folder so that its gmon.out file does not
    # conflict with the gmon.out file of ModelC (written to the current dir).
    pkill -f simbus
    mkdir $SIM_DIR/simbus
    (cd $SIM_DIR/simbus; $SIMBUS_CMD &)
    (cd $SIM_DIR; $MODELC_CMD)
    pkill -f simbus
}

generate
profile
