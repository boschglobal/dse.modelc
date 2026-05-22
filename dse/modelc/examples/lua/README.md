<!--
Copyright 2025 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# Lua Examples

Lua example models.


## Code Layout

### Source Code

```text
dse/modelc/
├── examples/
│   └── lua/
│       ├── cruise/                         Cruise control example.
│       │   ├── controller.lua              Controller model.
│       │   ├── plant.lua                   Plant model.
│       │   ├── cruise.lua                  Bindings to Lua Model interface.
│       │   ├── cruise.yaml
│       │   └── simulation.dse              Simulation definition (DSE Script).
│       ├── model.lua                       Simple Lua model example.
│       ├── modellib_api.lua                Lua Model Library API example.
│       ├── signalgroup.yaml
│       ├── simulation.yaml
│       └── README.md
```

### Deployed Examples

```text
dse/modelc/build/_out/
└── lib/
    └── lua/
        └── upy.lua                             <-- UPY Model Library (for Controller/Plant models)
└── examples/
    └── lua/
        ├── model/                              <-- Simple Lua model example.
        │   ├── data/
        │   │   └── simulation.yaml
        │   └── model/
        │       └── lua_inst/
        │           ├── model.lua
        │           └── data/
        │               └── signalgroup.yaml
        ├── cruise/                             <-- Lua Controller/Plant example.
        │   ├── controller.lua
        │   ├── plant.lua
        │   ├── cruise.lua
        │   ├── cruise.yaml
        │   └── simulation.dse
        └── modellib_api/                        <-- Lua Model Library API example.
            ├── data/
            │   └── simulation.yaml
            └── model/
                └── lua_inst/
                    ├── model.lua
                    └── data/
                        └── signalgroup.yaml
```


## Usage

### Example: Cruise Control

```bash
DSE_BUILDER_IMAGE=ghcr.io/boschglobal/dse-builder:latest
DSE_SIMER_IMAGE=ghcr.io/boschglobal/dse-simer:latest
export TASK_X_REMOTE_TASKFILES=1

# Build the simulation:
docker run -it --rm --user $(id -u):$(id -g) -v $(pwd):/workdir ${DSE_BUILDER_IMAGE} simulation.dse
task -y -t out/Taskfile.yml

# Run the simulation:
docker run -it --rm -v $(pwd)/out/sim:/sim ${DSE_SIMER_IMAGE}

# Convert the measurement:


```




```bash
# Get the repo.
$ git clone https://github.com/boschglobal/dse.modelc.git
$ cd dse.modelc

# Setup Simer.
$ export DSE_SIMER_IMAGE=ghcr.io/boschglobal/dse-simer:latest
$ simer() { ( cd "$1" && shift && docker run -it --rm -v $(pwd):/sim $DSE_SIMER_IMAGE "$@"; ) }

# Build.
$ make
```


### Model and Library API examples

```bash
# Run the Lua Model example.
$ simer dse/modelc/build/_out/examples/lua/model

# Run the Lua Model API example.
$ simer dse/modelc/build/_out/examples/lua/modellib_api
```
