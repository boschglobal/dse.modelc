---
title: "Simer - Simulation Runner"
linkTitle: "Simer"
weight: 20
tags:
- Simer
- CLI
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc"
---


## Synopsis

Containerised simulation run-time.

```bash
# Run a simulation.
$ simer path/to/simulation -stepsize 0.0005 -endtime 0.04
```



## Simulation Setup

### Structure

The structure of a simulation is flexible and can be arranged based on individual
project needs. All paths used within a simulation configuration should be
relative to the root of the simulation folder (i.e. the simulation path).
The `simer` tool will search for YAML files contained within the _simulation path_ and
automatically configure, and then run, the contained simulation.

A simulation can be further structured by using several _Stack definitions_ to
partition parts of the simulation, each with its own runtime configuration.


#### ModelC Example Simulation

```
# Example Source Code:
L- dse/modelc/examples/simer
  L- simulation.yaml          Stack definitions.
  L- signalgroup.yaml         Signal definitions.
  L- model.yaml               Model definition.
  L- model.c                  Model source code.
  L- CMakeLists.txt           Build script.

# Packaged Simulation:
L- dse/modelc/build/_out/examples/simer    <== simulation path
  L- data
    L- simulation.yaml        Stack definitions.
    L- signalgroup.yaml       Signal definitions.
    L- model.yaml             Model definition.
  L- lib
    L- libcounter.so          Model binary (shared library).
```


#### Manifest Generated Simulation

```
# Project repo (Taskfile/Manifest based):
L- <repo>/out/simulation/<name>
  L- simulation.yaml          Stack definitions.
  L- models                   Model instances.
    L- cssim
      L- data                 YAML files (Signal Groups etc).
      L- model                Model distribution (Model definition, binaries etc).
    L- input
      L- data                 YAML files (Signal Groups etc).
      L- model                Model distribution (Model definition, binaries etc).
    L- softecu
      L- data                 YAML files (Signal Groups etc).
      L- model                Model distribution (Model definition, binaries etc).
      L- softecu              Associated files (libraries, configuration, logs etc).
  L- data                     Additional data files (common to all models).
```


### x32 Models

Models which should run as x32 processes can be configured in a Stack via the
`ModelInstanceRuntime:x32` property. When the `x32` property of a model instance
is set to `true`, the `simer` tool will run that model using a x32 version of the ModelC runtime.

> Note: stacked models run in a single ModelC runtime instance. Therefore, when an `x32` property
is set for one model in a set of stacked models, the x32 versions of _all_ stacked models will
be executed by the x32 version of the ModelC runtime.


#### Example

<details>
<summary>simulation.yaml</summary>

```yaml
---
kind: Stack
metadata:
  name: simulation
spec:
  models:
    - name: x32_model_instance
      uid: 45
      model:
        name: X32_Model
      runtime:
        x32: true
      channels:
        - name: signal_channel
          alias: signal
```
</details>


### Remote Models

Simulations which contain remote models, such as Gateway models in foreign simulation environments or
Windows based models, can configure those remote models in a separate stack. When the simulation
is run by the `simer` tool, only selected stacks are started, and the remaining remote models
can then be started independently in their own environment.

> Hint: Models which need to be run with an older version of the `simer` tool can be defined in
their own stack and run independently (with an older version of Simer).


#### Example

<details>
<summary>simulation.yaml</summary>

```yaml
---
kind: Stack
metadata:
  name: core
spec:
  models:
    - name: simbus
      model:
        name: simbus
      channels:
        - name: signal_channel
          expectedModelCount: 2
    - name: model_instance
      uid: 44
      model:
        name: Model
      channels:
        - name: signal_channel
          alias: signal
---
kind: Stack
metadata:
  name: remote
spec:
  connection:
    transport:
      redispubsub:
        uri: redis://corehostname:6379
        timeout: 60
  models:
    - name: gateway_instance
      uid: 45
      model:
        name: GatewayModel
      channels:
        - name: signal_channel
          alias: signal
```
</details>


```bash
# Start the simulation (excluding Remote models).
$ simer path/to/simulation -stack core -endtime 0.1

# Now start the Remote models ...
```

### Stacked Models

Several models may be run stacked into a single ModelC runtime instance by setting
the `StackRuntime:stacked` property of their stack to `true`. The `simer` tool
will start _all_ models in that stack in a single instance of the ModelC runtime.

> Note: the `simbus` model, provided by the SimBus tool, is started independently of any runtime properties.


#### Example

<details>
<summary>simulation.yaml</summary>

```yaml
---
kind: Stack
metadata:
  name: simulation
spec:
  runtime:
    stacked: true
  models:
    - name: simbus
      model:
        name: simbus
      channels:
        - name: signal_channel
          expectedModelCount: 2
    - name: model_instance
      uid: 44
      model:
        name: Model
      channels:
        - name: signal_channel
          alias: signal
    - name: stacked_instance
      uid: 45
      model:
        name: Model
      channels:
        - name: signal_channel
          alias: signal
```
</details>


### Environment & Files

__Environment variables__ may be specified in either the `StackRuntime:env` or
`ModelInstanceRuntime:env` properties. The environment variables are passed
by the `simer` tool to the ModelC runtime in the order that they are
defined, `StackRuntime` environment variables first, and then
`ModelInstanceRuntime` variables. Values set later will overwrite earlier
values (or any existing environment variables).

__Additional Files__ may be specified in the `ModelInstanceRuntime:files` property. The path
of each listed file should be relative to _simulation path_ or an _absolute path_.


#### Example

<details>
<summary>simulation.yaml</summary>

```yaml
---
kind: Stack
metadata:
  name: simulation
spec:
  runtime:
    env:
      SIMBUS_LOGLEVEL: 4
  models:
    - name: simbus
      model:
        name: simbus
      channels:
        - name: data_channel
          expectedModelCount: 2
    - name: counter_A
      uid: 42
      model:
        name: Counter
      runtime:
        env:
          COUNTER_NAME: counter_A
          COUNTER_VALUE: 100
        files:
          - data/signalgroup.yaml
      channels:
        - name: data_channel
          alias: data
    - name: counter_B
      uid: 43
      model:
        name: Counter
      runtime:
        env:
          SIMBUS_LOGLEVEL: 3
          COUNTER_NAME: counter_B
          COUNTER_VALUE: 200
        files:
          - /tmp/sim/signalgroup.yaml
      channels:
        - name: data_channel
          alias: data
```
</details>



## Simmer Tool


### Setup

Simer is a containerised simulation run-time which can be setup using
a number of techniques. Container images are available and released with each
new version of the ModelC project.

```bash
# Latest Simer Container:
$ docker pull ghcr.io/boschglobal/dse-simer:latest

# Specific versions of the Simer Container
$ docker pull ghcr.io/boschglobal/dse-simer:2.0.7
$ docker pull ghcr.io/boschglobal/dse-simer:2.0
```

#### Shell Function

```bash
# Define a shell function (or add to .profile file).
$ export SIMER_IMAGE=ghcr.io/boschglobal/dse-simer:latest
$ simer() { ( cd "$1" && shift && docker run -it --rm -v $(pwd):/tmp $SIMER_IMAGE "$@"; ) }

# Run the simulation.
$ simer dse/modelc/build/_out/examples/minimal -endtime 0.04
```


### Options

```bash
$ simer -h
SIMER (Simulation Runner from Dynamic Simulation Environment)

  Containerised simulation run-time.

Examples:
  simer -endtime 0.1
  simer -stack minimal_stack -endtime 0.1 -logger 2
  simer -stepsize 0.0005 -endtime 0.005 -transport redis -uri redis://redis:6379
  simer -tmux -endtime 0.02

Flags:
  -endtime *flag.float64Value
        simulation end time (0.002)
  -gdb *flag.stringValue
        attach this model instance to GDB server
  -logger *flag.intValue
        log level (select between 0..4) (3)
  -stack *flag.stringValue
        run the named simulation stack(s)
  -stepsize *flag.float64Value
        simulation step size (0.0005)
  -timeout *flag.float64Value
        timeout (60)
  -tmux *flag.boolValue
        run simulation with TMUX user interface (false)
  -transport *flag.stringValue
        SimBus transport (redispubsub)
  -uri *flag.stringValue
        SimBus connection URI (redis://localhost:6379)
```



## Definitions


Channel
: Represents a grouping of signals which are exchanged between models.

Manifest
: A simulation project definition format which is used to define and build a simulation package.

Model (Definition)
: The definition of a model, specifying the location of libraries (for all supported os/arch combinations) as well as possible channel configurations.

Model (distribution/binary)
: A packaged version of a model (including binaries and related artifacts).

Model Instance
: An instance of a particular Model. Will include runtime properties.

ModelC
: An implementation of a Model Runtime (i.e. Importer).

Redis
: A data service used by ModelC instances to communicate with a SimBus. Redis is packaged in the Simer Container and started automatically (disabled by setting hidden parameter `-redis` to `""`).

Remote Model
: A model configured in a simulation but which runs on a remote system.

Signal Group
: A set, or subset, of signals which belong to a channel.

SimBus (Simulation Bus)
: An implementation of a data exchange mechanism for scalar and binary signals.

Simulation
: The structural arrangement of several Models, which, when connected via a Simulation Bus, form a simulation. Typically defined in a file `simulation.yaml`.

Simulation Package (simulation path/folder)
: This folder contains all configuration, models and artifacts necessary to run a simulation.

Stack
: Defines a list of models which should be run as a part of a simulation. Simulations may be defined by one or more stacks.

Stacked Models
: A list/collection of models which are stacked into a single ModelC runtime instance.

x32
: The linux x32 ABI architecture specifier, for models which must run as 32bit processes.
