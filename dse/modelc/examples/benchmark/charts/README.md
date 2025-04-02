<!--
Copyright 2025 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# Benchmark Charts

```bash
# Build local artifacts.
$ make
$ make build simer tools

# (optional) Also use local artifacts (i.e. simer container).
$ export export SIMER_IMAGE=simer:test

# Stop any local Redis.
$ sudo /etc/init.d/redis-server stop

# Run the chart generation scripts.
$ sh dse/modelc/examples/benchmark/charts/model_fanout.sh
$ sh dse/modelc/examples/benchmark/charts/signal_count.sh
$ sh dse/modelc/examples/benchmark/charts/signal_throughput.sh
```


## Codespaces

Run scripts under/with sudo `sudo`.


```bash
# Install google-chrome:
sudo add-apt-repository ppa:xtradeb/apps -y
sudo apt update
sudo apt install chromium
```
