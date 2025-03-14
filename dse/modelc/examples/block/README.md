<!--
Copyright 2025 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# Block Example

This model is used for transport correctness testing.


## Development

### Simer Operation

```bash
# Establish a simer command..
$ function dse-simer() { \
    ( if test -d "$1"; then cd "$1" && shift; fi \
    && docker run -it --rm \
        -v $(pwd):/sim \
        -e SIMBUS_LOGLEVEL=${SIMBUS_LOGLEVEL:-4} \
        -e SIMBUS_TRANSPORT=${SIMBUS_TRANSPORT:-redis} \
        -e SIMBUS_URI=${SIMBUS_URI:-redis://localhost} \
        -p 2159:2159 \
        -p 6379:6379 \
        $DSE_SIMER_IMAGE "$@"; ); }
$ export -f dse-simer

# Run using Simer with default transport and signals.
$ dse-simer dse/modelc/build/_out/examples/block

# Run using Simer with loopback transport.
# (this can also be done via edits to simulation.yaml)
$ SIMBUS_TRANSPORT=loopback \
  SIMBUS_URI=loopback \
  SIMBUS_LOGLEVEL=1 \
    dse-simer dse/modelc/build/_out/examples/block/

# Adjust the log level of the simulation.
# (fine grain control can be done with edits to simulation.yaml)
$ SIMBUS_LOGLEVEL=4 dse-simer dse/modelc/build/_out/examples/block/
```

### Generating Testdata

```bash
# Signals.
$ extra/tools/benchmark/bin/benchmark signalgroup -count 100 -output dse/modelc/examples/block/signal_group.yaml
```
