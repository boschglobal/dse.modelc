#!/bin/bash

# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


: "${SIMER_EXE:=/usr/local/bin/simer}"
: "${GATEWAY_EXE:=/usr/local/bin/gateway}"

# Remove the IP6 localhost entry from /etc/hosts.
sed 's/^::1.*localhost/::1\tip6-localhost/g' /etc/hosts > /etc/hosts.tmp
cat /etc/hosts.tmp > /etc/hosts

# Run the SIMER command.
SIMER_CMD="$SIMER_EXE"
echo "*** SIMER CMD: $SIMER_CMD $@"
$SIMER_CMD $@ &
#sleep 1

# Run the GATEWAY command.
GATEWAY_CMD="$GATEWAY_EXE"
echo "*** GATEWAY CMD: $GATEWAY_CMD $@"
$GATEWAY_CMD $@
