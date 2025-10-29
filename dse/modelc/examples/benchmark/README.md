<!--
Copyright 2025 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# Benchmark Example

This model is used for benchmark testing.


## Operation

```bash
$ git clone https://github.com/boschglobal/dse.modelc.git
$ cd dse.modelc
$ make
$ export SIMER_IMAGE=simer:test
$ make build simer tools

# Stop any local Redis servers.
$ sudo /etc/init.d/redis-server stop

# Run the benchmark script.
$ sh dse/modelc/examples/benchmark/scripts/benchmark.sh 5 2000 40
$ sh dse/modelc/examples/benchmark/scripts/benchmark.sh 5 2000 40 1 1
$ sh dse/modelc/examples/benchmark/scripts/benchmark.sh 5 2000 40 1 1 1 1 simer
$ sh dse/modelc/examples/benchmark/scripts/benchmark.sh 5 2000 40 1 1 1 1 fmi
...
Benchmark
=========
MODEL_COUNT=5
SIGNAL_COUNT=2000
STACKED=1 (opt: -stacked)
LOOPBACK=
MODEL_STEPSIZE=0.0005
MODEL_ENDTIME=1.0
MODEL_LOGGER=4

```


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
$ dse-simer dse/modelc/build/_out/examples/benchmark/

# Run using Simer with loopback transport.
# (this can also be done via edits to simulation.yaml)
$ SIMBUS_TRANSPORT=loopback \
  SIMBUS_URI=loopback \
  SIMBUS_LOGLEVEL=1 \
    dse-simer dse/modelc/build/_out/examples/benchmark/

# Adjust the log level of the simulation.
# (fine grain control can be done with edits to simulation.yaml)
$ SIMBUS_LOGLEVEL=4 dse-simer dse/modelc/build/_out/examples/benchmark/

```

### Generating Testdata

```bash
# Signals.
$ extra/tools/benchmark/bin/benchmark signalgroup -count 200 -output dse/modelc/examples/benchmark/signal_group.yaml

# Models.
$ extra/tools/benchmark/bin/benchmark signalgroup -count 200 -output dse/modelc/build/_out/examples/benchmark/data/signal_group.yaml
$ extra/tools/benchmark/bin/benchmark simulation -count 5 -stacked -output dse/modelc/build/_out/examples/benchmark/data/simulation.yaml

# Run the benchmark.
$ dse-simer dse/modelc/build/_out/examples/benchmark/ --endtime 1

$ SIMBUS_TRANSPORT=loopback \
  SIMBUS_URI=loopback \
    dse-simer dse/modelc/build/_out/examples/benchmark/ --endtime 1
```
