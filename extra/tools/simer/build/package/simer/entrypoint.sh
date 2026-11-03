#!/bin/bash

# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

: "${SIM_PATH:=}"
: "${SIMER_EXE:=/usr/local/bin/simer}"
: "${USER_ID:=${LOCAL_UID:-1000}}"
: "${GROUP_ID:=${LOCAL_GID:-1000}}"

# Switch to the provided user, if not `simer`.
if [ "$(id -g simer)" != "$GROUP_ID" ]; then
    groupmod -g "$GROUP_ID" simer
fi
if [ "$(id -u simer)" != "$USER_ID" ]; then
    usermod -u "$USER_ID" -g "$GROUP_ID" simer
    chown -R "$USER_ID:$GROUP_ID" /home/simer || true
fi

# Remove the IP6 localhost entry from /etc/hosts.
sed 's/^::1.*localhost/::1\tip6-localhost/g' /etc/hosts > /etc/hosts.tmp
cat /etc/hosts.tmp > /etc/hosts

# Change to the SIM_PATH if specified.
if [ -n "$SIM_PATH" ]; then cd $SIM_PATH; fi

# Run the SIMER command.
SIMER_CMD="$SIMER_EXE"
if [ "${SIMER_SCHED_FIFO:-0}" = "1" ]; then
    SIMER_RT_PRIO="${SIMER_RT_PRIO:-10}"
    exec chrt -f "$SIMER_RT_PRIO" gosu simer $SIMER_CMD "$@"
else
    exec gosu simer $SIMER_CMD "$@"
fi

# FIXME this change does not work in E2E tests.
#
# # Change to the SIM_PATH if specified.
# if [ -n "$SIM_PATH" ]; then cd $SIM_PATH; fi

# # Run the SIMER command.
# SIMER_CMD="$SIMER_EXE"
# if [ "$(id -u)" -eq 0 ]; then
#     exec gosu simer $SIMER_CMD "$@"
# else
#     exec $SIMER_CMD "$@"
# fi
