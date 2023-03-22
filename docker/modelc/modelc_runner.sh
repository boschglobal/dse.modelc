#!/bin/bash

# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

# Input Parameters
# ==========
#   MODEL_INSTANCE      - model instance name
#   MODEL_YAML          - list of configuration yaml files separated semicolon,
#                         as example "foo.yaml;bar.yaml"

# Parameters
# ==========
# some of these values may be pared from envars (if provided)

: "${MODEL_STEPSIZE:=0.005}"
: "${MODEL_ENDTIME:=0.02}"
: "${MODEL_LOGGER:=2}"
: "${MODEL_TIMEOUT:=60}"
if [ -n "$DEBUG" ]; then
    echo "ModelC Runner"
    echo "============="
    echo ""
    echo "Environment:"
    echo "------------"
    echo "MODEL_INSTANCE=${MODEL_INSTANCE}"
    echo "MODEL_YAML=${MODEL_YAML}"
    echo "MODEL_STEPSIZE=${MODEL_STEPSIZE}"
    echo "MODEL_ENDTIME=${MODEL_ENDTIME}"
    echo "MODEL_TIMEOUT=${MODEL_TIMEOUT}"
    echo "MODEL_LOGGER=${MODEL_LOGGER}"
    echo "SIMBUS_URI=${SIMBUS_URI}"
    echo "SIMBUS_TRANSPORT=${SIMBUS_TRANSPORT}"
    echo "SIMBUS_LOGLEVEL=${SIMBUS_LOGLEVEL}"
fi

: "${MODELC_EXE:=/usr/local/bin/modelc}"
: "${MODEL_DIR:=/tmp/sim}"
mkdir -p $MODEL_DIR

# ModelC Command
# ==============
IFS=";" read -ra MODEL_YAML_ARR <<< "$MODEL_YAML"
MODELC_CMD="$MODELC_EXE --stepsize ${MODEL_STEPSIZE} --endtime ${MODEL_ENDTIME} --name ${MODEL_INSTANCE} --logger $MODEL_LOGGER --timeout $MODEL_TIMEOUT ${MODEL_YAML_ARR[@]}"

if [ -n "$DEBUG" ]; then
    echo ""
    echo "Final run conditions:"
    echo "---------------------"
    echo "MODEL_DIR=${MODEL_DIR}"
    echo "MODELC_CMD=${MODELC_CMD}"
fi

cd $MODEL_DIR
$MODELC_CMD

if [ $? -eq 139 ]; then
    echo ""
    echo "!!! CORE DUMP !!!"
    echo "-----------------"
    echo "  To debug Core dumps, configure host with cmd:"
    echo "  > echo '/tmp/core.%e.%p' | sudo tee /proc/sys/kernel/core_pattern"
    echo "MODEL_DIR=${MODEL_DIR}"
    echo "MODELC_CMD=${MODELC_CMD}"
    echo "WORKING:DIR=$(pwd)"
    ls /tmp
    echo "-----------------"
    gdb -q $MODELC_EXE /tmp/core.$(basename $MODELC_EXE).* -ex 'set pagination off' -ex 'backtrace' -ex 'quit'
    echo "-----------------"
    exit 139
fi
