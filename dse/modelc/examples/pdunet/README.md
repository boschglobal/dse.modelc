<!--
Copyright 2026 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# PDU Network Examples

PDU Net example models.


## Code Layout

### Source Code

```text
dse/modelc
└── examples/pdunet
    └── frnet
        └── model.c                     <-- Model code.
        └── model.yaml                  <-- Model definition.
        └── network.yaml                <-- FlexRay network definition.
        └── simulation.yaml             <-- Stack and Signal Group definitions.
    └── luafrnet
        └── model.lua                   <-- Lua model script.
        └── network.yaml                <-- FlexRay network definition.
        └── simulation.yaml             <-- Stack and Signal Group definitions.
    └── container
        └── model.c                     <-- Container PDU example model code.
        └── model.yaml                  <-- Model definition.
        └── network.yaml                <-- FlexRay network definition.
        └── simulation.yaml             <-- Stack and Signal Group definitions.
    └── sec
        └── model.c                     <-- Secure PDU example model code.
        └── model.yaml                  <-- Model definition.
        └── network.yaml                <-- FlexRay network definition.
        └── simulation.yaml             <-- Stack and Signal Group definitions.
    └── README.md
```

### Deployed Examples

```text
dse/modelc/build/_out
└── examples/pdunet
    └── frnet                           <-- Simple FlexRay network example.
        └── data
            └── model.yaml
            └── network.yaml
            └── simulation.yaml
        └── lib
            └── libfrnet.so
    └── luafrnet                        <-- Simple Lua version of FlexRay example.
        └── data
            └── simulation.yaml
        └── model/pdunet_inst
            └── model.lua
            └── data
                └── network.yaml
    └── container                       <-- Container PDU network example.
        └── data
            └── model.yaml
            └── network.yaml
            └── simulation.yaml
        └── lib
            └── libcontainer.so
    └── sec                             <-- Secure PDU network example.
        └── data
            └── model.yaml
            └── network.yaml
            └── simulation.yaml
        └── lib
            └── libsec.so
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

# Run the simple FlexRay example.
$ simer dse/modelc/build/_out/examples/pdunet/frnet

# Run the simple Lua FlexRay example.
$ simer dse/modelc/build/_out/examples/pdunet/luafrnet
```
