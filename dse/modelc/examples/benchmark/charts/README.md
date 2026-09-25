<!--
Copyright 2025 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# Benchmark Charts

## WSL

### Setup

Add to file /etc/docker/daemon.json
```json
{
  "cpu-rt-runtime": 950000
}
```

Restart docker
```bash
sudo systemctl restart docker
```

Validate:
```bash
docker run --rm \
  --cap-add=SYS_NICE \
  --ulimit rtprio=99 \
  --cpu-rt-runtime=950000 \
  --entrypoint='' simer:test \
  sh -c 'chrt -f 10 true && echo chrt-ok'
```


### Commands

```bash
# Build local artifacts.
$ make build tools
# Associated FMI repo.
$ make build fmi tools

# (optional) Also use local artifacts (i.e. simer container).
$ export SIMER_IMAGE=simer:test
$ export FMI_IMAGE=fmi:test

# Stop any local Redis.
$ sudo /etc/init.d/redis-server stop

# Run the chart generation scripts.
$ sh dse/modelc/examples/benchmark/charts/model_fanout.sh
$ sh dse/modelc/examples/benchmark/charts/signal_count.sh
$ sh dse/modelc/examples/benchmark/charts/signal_throughput.sh
$ sh dse/modelc/examples/benchmark/charts/startup_signal.sh
$ sh dse/modelc/examples/benchmark/charts/modelcfmu_signal_count.sh
```


## Codespaces

Run scripts under/with sudo `sudo`.


```bash
# Install google-chrome:
sudo add-apt-repository ppa:xtradeb/apps -y
sudo apt update
sudo apt install chromium
```
