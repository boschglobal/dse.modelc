<!--
Copyright 2025 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# Model GO


## Usage

### Gateway

```bash
# Define a shell function (or add to .profile file).
$ gateway() { ( cd "$1" && shift && docker run -it --rm -v $(pwd):/sim gateway:test "$@"; ) }

# Run the simulation.
$ gateway examples/gateway/sim
```


## Development

```bash
# Build and test.
$ cd extra/go/modelgo
$ make
$ make test
$ make test_e2e

# Test with a local Simer image.
$ SIMER=simer:test make test_e2e

# Build and package the examples.
$ make examples
$ make docker
```


### Go Modules

### Debug with Containers

Edit the examples/gateway/Dockerfile to use `simer:test` as basis for final image.

```bash
# Build the image.
$ docker rmi gateway:test; make docker

# Start the container, bypassing the entrypoint script.
$ docker run -it --rm --entrypoint=/bin/bash  -v ./examples/gateway/sim:/sim gateway:test

# Run these commands inside the container; fill the redis key, then consume.
$ export NCODEC_TRACE_PDU_0=*
$ /usr/local/bin/redis-server /usr/local/etc/redis/redis.conf &
$ /usr/local/bin/gateway -logger 0
$ /usr/local/bin/simbus --logger 0 --stepsize 0.0005 data/simulation.yaml
```


### End2End Testing

Install Test Containers with this method:

```bash
export GOINSECURE=""
export GOPROXY="direct"
go get github.com/testcontainers/testcontainers-go
```
