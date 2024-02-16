<!--
Copyright 2024 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# SIMER : Simulation Runner from Dynamic Simulation Environment

Containerised simulation run-time.


## Usage

```bash
# Define a shell function (or add to .profile file).
$ simer() { ( cd "$1" && shift && docker run -it --rm -v $(pwd):/tmp simer:test "$@"; ) }

# Run the simulation.
$ simer dse/modelc/build/_out/examples/minimal -endtime 0.04
```

## Integration Testing

### Building Artifacts

```bash
# Clear the Simer build folder.
$ rm -rvf extra/tools/simer/build/stage

# Build x32 targets.
$ make cleanall
$ PACKAGE_ARCH=linux-x86 make build simer

# Build x64 targets.
$ make cleanall
$ make build simer

# Modify the filesystem (of the example model) for testing.
$ mkdir -p dse/modelc/build/_out/examples/simer/lib32
$ cp extra/tools/simer/build/stage/lib32/libcounter.so dse/modelc/build/_out/examples/simer/lib32/libcounter.so

# Build the Simer container.
$ ls extra/tools/simer/build/stage/*
$ make tools
```


### Native Testing

```bash
$ cd extra/tools/simer
$ make run_noredis
```


### Container Testing

```bash
$ simer() { ( cd "$1" && shift && docker run -it --rm -v $(pwd):/tmp simer:test "$@"; ) }
$ simer dse/modelc/build/_out/examples/simer -endtime 0.04
```


## Development

### Go Module Update (schema updates)

```bash
$ export GOPRIVATE=github.com/boschglobal
$ go clean -modcache
$ go mod tidy
$ go get -x github.com/boschglobal/dse.schemas/code/go/dse@v1.2.6
```

> Note: Release Tags for modules in DSE Schemas are according to the schema `code/go/dse/v1.2.6`.


### Container Debug

```bash
# Start the container.
$ docker run --entrypoint /bin/bash -it --rm -v .:/tmp simer:test

# Check Redis operation.
$ /usr/local/bin/redis-server /usr/local/etc/redis/redis.conf
$ /usr/local/bin/redis-server /usr/local/etc/redis/redis.conf &
$ /usr/local/bin/redis-cli -h localhost

# Check Simer/Tool operation
# (Modify /etc/hosts before running the following commands)
$ /usr/local/bin/simer
$ /usr/local/bin/simbus
```
