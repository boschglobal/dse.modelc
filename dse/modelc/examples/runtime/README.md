<!--
Copyright 2024 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# Model Runtime Example

The example implementation of a Model Runtime includes:

* __Importer__ - implements a simple importer for the ModelC (model.h) interface.
* __Runtime__ - an implementation of the importer interface which calls the
  ModelC Model Runtime (runtime.h).
* __Simulation__ - single model simulation (Simer style layout).


## Project Structure

```text
L- dse/modelc           Model C Library source code.
  L- model.h            Model interface.
  L- runtime.h          Runtime interface.
  L- controller
    L- model_runtime.c  Model Runtime implementation.
  L- examples/runtime
    L- importer.c       Example importer (implements model.h).
    L- runtime.c        Example importer/runtime model.
    L- sim              Example simulation with Simer style layout.
      L- model.c        Example ModelC model.
```


## Run the Example

```bash
# Clone and build the project:
$ git clone https://github.com/boschglobal/dse.modelc.git
$ cd dse.modelc
$ make

# Run the example:
$ dse/modelc/build/_out/examples/runtime/bin/importer \
    dse/modelc/build/_out/examples/runtime/lib/libruntime.so \
    dse/modelc/build/_out/examples/runtime/sim \
    data/simulation.yaml \
    target_inst
Importer: Cwd: /mnt/c/Users/rut6abt/Desktop/git/working/dse.modelc
Importer: Runtime: dse/modelc/build/_out/examples/runtime/lib/libruntime.so
Importer: Simulation Path: dse/modelc/build/_out/examples/runtime/sim
Importer: Simulation YAML: data/simulation.yaml
Importer: Model: target_inst
Importer: Loading runtime model: dse/modelc/build/_out/examples/runtime/lib/libruntime.so ...
Importer: vtable.create: 0x7f270f14fc10
Importer: vtable.step: 0x7f270f14fc30
Importer: vtable.destroy: 0x7f270f14fc40
Importer: Calling vtable.create() ...
Runtime: Version: 0.0.1
Runtime: Platform: linux-amd64
Runtime: Time: 2024-06-10 12:36:11 (UTC)
Runtime: Host: ABTZ0NKR
Runtime: CWD: /mnt/c/Users/rut6abt/Desktop/git/working/dse.modelc
Runtime: Simulation Path: dse/modelc/build/_out/examples/runtime/sim
Runtime: Simulation YAML: dse/modelc/build/_out/examples/runtime/sim/data/simulation.yaml
Runtime: Model: target_inst
Load YAML File: dse/modelc/build/_out/examples/runtime/sim/data/simulation.yaml
Load YAML File: dse/modelc/build/_out/examples/runtime/sim/data/model.yaml
...
```


## Linking Strategy

### Importer using Dynamic Loading

In this scenario the Runtime is loaded (by the importer) using `dlopen()` with
flags `RTLD_NOW` and `RTLD_LOCAL`. Runtime is dynamically linked to the
ModelC shared library (`libmodelc.so`) and this library is loaded at this point.
Models which are _also_ dynamically linked to the ModelC shared library, when
loaded by the Runtime, will also use the previously loaded `libmodelc.so`.


![importer-local](../../../../doc/static/examples-runtime-importer-local.png)


## Developer

```bash
# Use GDB:
$ gdb -q -ex='set confirm on' -ex=run -ex=quit --args \
    dse/modelc/build/_out/examples/runtime/bin/importer \
    dse/modelc/build/_out/examples/runtime/lib/libruntime.so \
    dse/modelc/build/_out/examples/runtime/sim \
    data/simulation.yaml \
    target_inst

# Use Valgrind:
$ valgrind -q --leak-check=yes \
    dse/modelc/build/_out/examples/runtime/bin/importer \
    dse/modelc/build/_out/examples/runtime/lib/libruntime.so \
    dse/modelc/build/_out/examples/runtime/sim \
    data/simulation.yaml \
    target_inst

# Inspect Runtime linking.
$ readelf -d lib/libruntime.so

Dynamic section at offset 0x2df0 contains 27 entries:
  Tag        Type                         Name/Value
 0x0000000000000001 (NEEDED)             Shared library: [libmodelc.so]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
 0x000000000000000e (SONAME)             Library soname: [libruntime.so]
 0x000000000000001d (RUNPATH)            Library runpath: [lib]
 ....
```
