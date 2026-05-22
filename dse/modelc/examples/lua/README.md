<!--
Copyright 2026 Robert Bosch GmbH

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
git clone https://github.com/boschglobal/dse.modelc.git
cd dse.modelc; make
cd dse/modelc/build/_out/examples/lua/cruise

# Setup the DSE environment:
source <(curl -fsSL https://raw.githubusercontent.com/boschglobal/dse.sdp/main/setup.sh)

# Build the simulation:
dse-builder simulation.dse
task -y -t out/Taskfile.yml

# Run the simulation:
dse-simer -endtime 10.0

# Extract measurement files from the trace:
dse-trace convert --csv data/simbus.bin
```


### Example: Model Library API

```bash
git clone https://github.com/boschglobal/dse.modelc.git
cd dse.modelc; make

# Setup the DSE environment:
source <(curl -fsSL https://raw.githubusercontent.com/boschglobal/dse.sdp/main/setup.sh)

# Run the Lua Model example:
dse-simer dse/modelc/build/_out/examples/lua/model

# Run the Lua Model API example:
dse-simer dse/modelc/build/_out/examples/lua/modellib_api
```
