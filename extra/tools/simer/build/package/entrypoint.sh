#!/bin/bash

# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


: "${SIMER_EXE:=/usr/local/bin/simer}"

# Remove the IP6 localhost entry from /etc/hosts.
sed 's/^::1.*localhost/::1\tip6-localhost/g' /etc/hosts > /etc/hosts.tmp
cat /etc/hosts.tmp > /etc/hosts

# Change to the SIM_PATH if specified.
if [ -n "$SIM_PATH" ]; then cd $SIM_PATH; fi

# Run the SIMER command.
SIMER_CMD="$SIMER_EXE"
$SIMER_CMD $@
