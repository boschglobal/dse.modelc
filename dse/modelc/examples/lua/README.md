<!--
Copyright 2025 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# Lua Examples

Lua example models.


## Code Layout

### Source Code

```text
dse/modelc
└── examples/lua
    └── model.lua                       <-- Simple Lua model.
    └── modellib_api.lua                <-- Lua Model Library API model.
    └── signalgroup.yaml
    └── simulation.yaml
    └── README.md
```

### Deployed Examples

```text
dse/modelc/build/_out
└── examples/lua
    └── model                           <-- Simple Lua model example.
        └── data
            └── simulation.yaml
        └── model/lua_inst
            └── model.lua
            └── data
                └── signalgroup.yaml
    └── modellib_api                    <-- Lua Model Library API example.
        └── data
            └── simulation.yaml
        └── model/lua_inst
            └── model.lua
            └── data
                └── signalgroup.yaml
```


## Operation

```bash
# Get the repo.
$ git clone https://github.com/boschglobal/dse.modelc.git
$ cd dse.modelc

# Setup Simer.
$ export DSE_SIMER_IMAGE=ghcr.io/boschglobal/dse-simer:latest
$ simer() { ( cd "$1" && shift && docker run -it --rm -v $(pwd):/sim $DSE_SIMER_IMAGE "$@"; ) }

# Build.
$ make

# Run the Lua Model example.
$ simer dse/modelc/build/_out/examples/lua/model

# Run the Lua Model API example.
$ simer dse/modelc/build/_out/examples/lua/modellib_api
```
