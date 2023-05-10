<!--
Copyright 2023 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

### Run the Example

This example runs the Gateway using a containerised SimBus and ModelC runtime
environment. There are numerous other ways to configure and run simulations.


#### Build the Project

```bash
$ git clone https://github.com/boschglobal/dse.clib.git
$ cd dse.clib
$ make
```


#### Setup SimBus

```bash
# Create a network for the simulation.
$ docker network create dse

# Start Redis (in the background).
$ docker run --rm -it --name redis --net dse -p 6379:6379 redis &

# Run the SimBus (restart for each simulation run).
$ docker run --rm -it --name simbus --net dse \
        --volume $(pwd)/dse/modelc/build/_out/examples/gateway:/tmp \
        --env SIMBUS_YAML="/tmp/data/gateway.yaml" \
        ghcr.io/boschglobal/dse-simbus-sa:v1.0.1
```

The Docker images can also be build locally with the `make docker` target.


##### Run the Gateway

```bash
# Start a container.
$ docker run --rm -it --name gateway --net dse \
        --volume $(pwd)/dse/modelc/build/_out/examples/gateway:/tmp \
        ghcr.io/boschglobal/dse-gcc-builder:main

# Run the Gateway
$ cd /tmp/bin
$ bin/gateway 0.005 0.02 data/gateway.yaml
Load YAML File: data/gateway.yaml
Simulation Parameters:
  Step Size: 0.005000
  End Time: 0.020000
  Model Timeout: 60.000000
Transport:
  Transport: redispubsub
  URI: redis://redis:6379
Platform:
  Platform OS: linux
  Platform Arch: amd64
Model Instance:
  Name: gateway
  UID: 42
  Model Name: Gateway
  Model Path: (null)
  Model File: (null)
  Model Location: (null)
Create the Endpoint object ...
  Redis Pub/Sub:
    path: (null)
    hostname: redis
    port: 6379
  Endpoint:
    Model UID: 35
    Client ID: 35
Create the Controller object ...
Create the Adapter object ...
Load and configure the Simulation Models ...
Using gateway symbols: ...
Call symbol: model_setup ...
Configure Channel: scalar_vector
  Channel Name: scalar
  Channel Alias: scalar_vector
  Unique signals identified: 2
Init Controller channel: scalar
  Pub Key: bus.ch.scalar.tx
  Sub Key: bus.ch.scalar.rx
Configure Channel: binary_vector
  Channel Name: binary
  Channel Alias: binary_vector
  Unique signals identified: 2
Init Controller channel: binary
  Pub Key: bus.ch.binary.tx
  Sub Key: bus.ch.binary.rx
Setup for async Simulation Model run ...
[INFO]   [0.000000] binary[0] = <0:0>(null) (binary_foo) (main:55)
[INFO]   [0.000000] binary[1] = <0:0>(null) (binary_bar) (main:55)
[INFO]   [0.000000] scalar[0] = 0.000000 (scalar_foo) (main:67)
[INFO]   [0.000000] scalar[1] = 0.000000 (scalar_bar) (main:67)
[INFO]   [0.005000] binary[0] = <23:23>st=0.000000,index=0 (binary_foo) (main:55)
[INFO]   [0.005000] binary[1] = <23:23>st=0.000000,index=1 (binary_bar) (main:55)
[INFO]   [0.005000] scalar[0] = 4.000000 (scalar_foo) (main:67)
[INFO]   [0.005000] scalar[1] = 8.000000 (scalar_bar) (main:67)
[INFO]   [0.010000] binary[0] = <23:23>st=0.005000,index=0 (binary_foo) (main:55)
[INFO]   [0.010000] binary[1] = <23:23>st=0.005000,index=1 (binary_bar) (main:55)
[INFO]   [0.010000] scalar[0] = 8.000000 (scalar_foo) (main:67)
[INFO]   [0.010000] scalar[1] = 16.000000 (scalar_bar) (main:67)
[INFO]   [0.015000] binary[0] = <23:23>st=0.010000,index=0 (binary_foo) (main:55)
[INFO]   [0.015000] binary[1] = <23:23>st=0.010000,index=1 (binary_bar) (main:55)
[INFO]   [0.015000] scalar[0] = 12.000000 (scalar_foo) (main:67)
[INFO]   [0.015000] scalar[1] = 24.000000 (scalar_bar) (main:67)
[INFO]   [0.020000] binary[0] = <23:23>st=0.015000,index=0 (binary_foo) (main:55)
[INFO]   [0.020000] binary[1] = <23:23>st=0.015000,index=1 (binary_bar) (main:55)
[INFO]   [0.020000] scalar[0] = 16.000000 (scalar_foo) (main:67)
[INFO]   [0.020000] scalar[1] = 32.000000 (scalar_bar) (main:67)
Call symbol: model_exit ...
Controller exit ...
```
