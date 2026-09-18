#!/bin/sh

# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


# Benchmark profiling
# ====================
#
# Setup:
#
#   $ make clean
#   $ make build tools
#   $ sudo service redis-server start
#
# Run the benchmark with Callgrind (the default):
#
#   $ sh dse/modelc/examples/benchmark/scripts/profile.sh
#
# Select another profiler with PROFILE:
#
#   $ PROFILE=cachegrind sh dse/modelc/examples/benchmark/scripts/profile.sh
#   $ PROFILE=gdb sh dse/modelc/examples/benchmark/scripts/profile.sh
#
# View the newest profiler output:
#
#   $ sh dse/modelc/examples/benchmark/scripts/profile.sh view
#   $ sh dse/modelc/examples/benchmark/scripts/profile.sh view tree
#   $ PROFILE=cachegrind sh dse/modelc/examples/benchmark/scripts/profile.sh view
#
# Callgrind view modes are inclusive (default), tree, and code. Use `help`
# for the complete command list.

ENTRY_DIR=$(pwd)


help()
{
        cat <<'EOF'
Usage: profile.sh [help|view [mode]]

Select the profiling command with PROFILE. The default is PROFILE=callgrind.

    PROFILE=cachegrind  Valgrind Cachegrind; use cg_annotate for analysis.
    PROFILE=callgrind   Valgrind Callgrind; use callgrind_annotate for analysis.
    PROFILE=gdb         GNU Debugger; runs ModelC with GDB.

Examples:
    PROFILE=callgrind ./profile.sh
    PROFILE=cachegrind ./profile.sh
    PROFILE=gdb ./profile.sh

View the newest profile output:
    PROFILE=callgrind ./profile.sh view
    PROFILE=callgrind ./profile.sh view inclusive
    PROFILE=callgrind ./profile.sh view tree
    PROFILE=callgrind ./profile.sh view code
    PROFILE=cachegrind ./profile.sh view

Callgrind view modes:
    inclusive  Inclusive costs (default).
    tree       Caller/callee tree.
    code       Source-level cost annotations.
EOF
}

view()
{
        case "${PROFILE}" in
            cachegrind)
                profile_file=$(ls -1t "${ENTRY_DIR}"/cachegrind.out.* \
                    2>/dev/null | sed -n '1p')
                if [ -z "${profile_file}" ]; then
                    echo "No cachegrind output found in ${ENTRY_DIR}" >&2
                    return 1
                fi
                cg_annotate --threshold=1 \
                    --include="${ENTRY_DIR}/" "${profile_file}"
                ;;
            callgrind)
                profile_file=$(ls -1t "${ENTRY_DIR}"/callgrind.out.* \
                    2>/dev/null | sed -n '1p')
                if [ -z "${profile_file}" ]; then
                    echo "No callgrind output found in ${ENTRY_DIR}" >&2
                    return 1
                fi
                sed -i "s|/tmp/repo/dse/modelc|${ENTRY_DIR}/dse/modelc|g" \
                    "${profile_file}"
                : "${VIEW_MODE:=inclusive}"
                case "${VIEW_MODE}" in
                    inclusive)
                        callgrind_annotate --inclusive=yes --auto=no \
                            --include="${ENTRY_DIR}/" "${profile_file}" |
                            grep -vE 'ld-linux|libc\.so|libc_start|:main' |
                            awk '($1 != last_count || $1 == "") {
                                print
                                last_count = $1
                            }'
                        ;;
                    tree)
                        callgrind_annotate --tree=caller --auto=no \
                            --include="${ENTRY_DIR}/" "${profile_file}"
                        ;;
                    code)
                        callgrind_annotate --threshold=1 \
                            --include="${ENTRY_DIR}/" "${profile_file}"
                        ;;
                    *)
                        printf 'Unknown Callgrind view mode: %s\n\n' \
                            "${VIEW_MODE}" >&2
                        help >&2
                        return 2
                        ;;
                esac
                ;;
            gdb)
                echo "GDB does not produce an annotation file to view." >&2
                return 1
                ;;
        esac
}

if [ "${1:-}" = "help" ] || [ "${1:-}" = "-h" ] ||
    [ "${1:-}" = "--help" ]; then
        help
        exit 0
fi

if [ "${1:-}" = "view" ]; then
        : "${PROFILE:=callgrind}"
    : "${VIEW_MODE:=${2:-inclusive}}"
        case "${PROFILE}" in
            cachegrind|callgrind|gdb) ;;
            *)
                printf 'Unknown PROFILE: %s\n\n' "${PROFILE}" >&2
                help >&2
                exit 2
                ;;
        esac
        view
        exit $?
fi


# Parameters
# ==========
: "${SIM_DIR:=dse/modelc/build/_out/examples/benchmark}"
: "${MODEL_COUNT:=5}"
: "${SIGNAL_COUNT:=2000}"
: "${SIGNAL_CHANGE:=2000}"
: "${MODEL_STEPSIZE:=0.0005}"
: "${MODEL_ENDTIME:=1.0}"

: "${SIMBUS_EXE:=../../../bin/simbus}"
: "${MODELC_EXE:=../../bin/modelc}"
: "${SIMBUS_LOGGER:=4}"
: "${MODELC_LOGGER:=4}"

: "${SIMBUS_TRANSPORT=loopback}"
: "${SIMBUS_URI=loopback://localhost}"

: "${PROFILE:=callgrind}"

case "${PROFILE}" in
    cachegrind)
        ;;
    callgrind)
        ;;
    gdb)
        ;;
    *)
        printf 'Unknown PROFILE: %s\n\n' "${PROFILE}" >&2
        help >&2
        exit 2
        ;;
esac

MODEL_NAMES="benchmark_inst_1;benchmark_inst_2;benchmark_inst_3;benchmark_inst_4;benchmark_inst_5"
YAML_FILES="data/simulation.yaml data/signal_group.yaml data/model.yaml"
SIMBUS_CMD="$SIMBUS_EXE --name simbus --stepsize ${MODEL_STEPSIZE} --logger ${SIMBUS_LOGGER} ../data/simulation.yaml"

run_modelc()
{
    case "${PROFILE}" in
        cachegrind)
            valgrind --tool=cachegrind \
                --cachegrind-out-file="${ENTRY_DIR}/cachegrind.out.%p" \
                "${MODELC_EXE}" \
                --stepsize "${MODEL_STEPSIZE}" \
                --endtime "${MODEL_ENDTIME}" \
                --name "${MODEL_NAMES}" \
                --logger "${MODELC_LOGGER}" \
                ${YAML_FILES}
            ;;
        callgrind)
            valgrind --tool=callgrind \
                --callgrind-out-file="${ENTRY_DIR}/callgrind.out.%p" \
                "${MODELC_EXE}" \
                --stepsize "${MODEL_STEPSIZE}" \
                --endtime "${MODEL_ENDTIME}" \
                --name "${MODEL_NAMES}" \
                --logger "${MODELC_LOGGER}" \
                ${YAML_FILES}
            ;;
        gdb)
            gdb -q -ex 'set confirm on' -ex run -ex quit -args \
                "${MODELC_EXE}" \
                --stepsize "${MODEL_STEPSIZE}" \
                --endtime "${MODEL_ENDTIME}" \
                --name "${MODEL_NAMES}" \
                --logger "${MODELC_LOGGER}" \
                ${YAML_FILES}
            ;;
    esac
}


generate()
{
    extra/tools/benchmark/bin/benchmark signalgroup -count ${SIGNAL_COUNT} -output dse/modelc/build/_out/examples/benchmark/data/signal_group.yaml
    extra/tools/benchmark/bin/benchmark simulation -count ${MODEL_COUNT} -stacked -output dse/modelc/build/_out/examples/benchmark/data/simulation.yaml
}

cleanup()
{
    pkill -f simbus
    if [ ! $SIMBUS_TRANSPORT = "loopback" ]; then
        redis-cli flushall
    fi
}

profile()
{
    # Run the SimBus in the sim/lib folder so that its gmon.out file does not
    # conflict with the gmon.out file of ModelC (written to the current dir).
    mkdir -p $SIM_DIR/simbus
    if [ ! $SIMBUS_TRANSPORT = "loopback" ]; then
        (cd $SIM_DIR/simbus; $SIMBUS_CMD &)
    fi
    (cd "$SIM_DIR" && run_modelc)
}

export SIMBUS_TRANSPORT=${SIMBUS_TRANSPORT}
export SIMBUS_URI=${SIMBUS_URI}
export SIGNAL_CHANGE=${SIGNAL_CHANGE}

cleanup
generate
profile
cleanup
