---
title: "Trace - Simulation Trace Tool"
linkTitle: "Trace"
weight: 100
tags:
- trace
- CLI
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc"
---


## Synopsis

Simulation trace tool.

```bash
# Create the trace folder.
$ mkdir examples/binary/data/trace

# Collect a SimBus trace from a simulation run.
$ docker run --name simer -i --rm \
    --volume ./examples/binary:/sim \
    --env SIMBUS_TRACEFILE=data/trace/simbus.bin \
    ghcr.io/boschglobal/dse-simer:latest

# Print a long-form summary of the trace file.
$ docker run --name simer -i --rm \
    --volume ./examples/binary:/sim \
    --entrypoint /usr/local/bin/trace \
    ghcr.io/boschglobal/dse-simer:latest
        summary -long data/trace/simbus.bin

```


## Commands

The Trace tool includes the following commands and options:

```bash
Trace tools for working with SimBus trace files.

Usage:

        trace <command> [option] <trace file>

        trace summary [--short, --long] <trace file>

  summary
    Options:
      -long *flag.boolValue
          generate a long summary  (default: false)
      -short *flag.boolValue
          generate a short summary  (default: true)
```

> __Note__: The `trace` command is included with the Simer container image
(installed to location `/usr/local/bin/trace`).


### Summary

Prints a summary of the SimBus trace messages contained in a trace file to
the console. Several options are available to control the message formatting.

#### Option Short (-short)

Print a short summary of messages contained in a SimBus trace file.

<details>
<summary>Example</summary>

```bash
$ trace summary -short data/trace/simbus.bin
scalar_channel:42:1:0::ModelRegister
binary_channel:42:1:0::ModelRegister
scalar_channel:42:2:0::ModelRegister
binary_channel:42:2:0::ModelRegister
scalar_channel:42:0:0::SignalIndex
binary_channel:42:0:0::SignalIndex
scalar_channel:42:0:0::SignalRead
binary_channel:42:0:0::SignalValue
scalar_channel:42:0:0::SignalIndex
binary_channel:42:0:0::SignalIndex
scalar_channel:42:0:0::SignalRead
binary_channel:42:0:0::SignalValue
Notify:0.000000:0.000000:0.000000 (S)<--(M) (42)
Notify:0.000000:0.000000:0.000500 (S)-->(M) (0)
Notify:0.000500:0.000000:0.000000 (S)<--(M) (42)
Notify:0.000500:0.000000:0.001000 (S)-->(M) (0)
Notify:0.001000:0.000000:0.000000 (S)<--(M) (42)
Notify:0.001000:0.000000:0.001500 (S)-->(M) (0)
Notify:0.001500:0.000000:0.000000 (S)<--(M) (42)
Notify:0.001500:0.000000:0.002000 (S)-->(M) (0)
Notify:0.002000:0.000000:0.000000 (S)<--(M) (42)
Notify:0.002000:0.000000:0.002500 (S)-->(M) (0)
scalar_channel:42:0:0::ModelExit
scalar_channel:42:0:0::ModelExit
```
</details>


#### Option Long (-long)

Print a longer summary of messages contained in a SimBus trace file.

<details>
<summary>Example</summary>

```bash
$ trace summary -long data/trace/simbus.bin
scalar_channel:42:1:0::ModelRegister
binary_channel:42:1:0::ModelRegister
...
[0.000000] Notify:0.000000:0.000000:0.000000 (S)<--(M) (42)
[0.000000] Notify[42]:SignalVector:scalar_channel
[0.000000] Notify[42]:SignalVector:scalar_channel:Signal:counter=42.000000
[0.000000] Notify[42]:SignalVector:binary_channel
[0.000000] Notify:0.000000:0.000000:0.000500 (S)-->(M) (0)
[0.000000] Notify[8000008]:SignalVector:scalar_channel
[0.000000] Notify[8000008]:SignalVector:scalar_channel:Signal:counter=42.000000
[0.000000] Notify[8000008]:SignalVector:binary_channel
[0.000500] Notify:0.000500:0.000000:0.000000 (S)<--(M) (42)
[0.000500] Notify[42]:SignalVector:scalar_channel
[0.000500] Notify[42]:SignalVector:scalar_channel:Signal:counter=43.000000
[0.000500] Notify[42]:SignalVector:binary_channel
[0.000500] Notify[42]:SignalVector:binary_channel:Signal:message=len(12)
[0.000500] Notify:0.000500:0.000000:0.001000 (S)-->(M) (0)
[0.000500] Notify[8000008]:SignalVector:scalar_channel
[0.000500] Notify[8000008]:SignalVector:scalar_channel:Signal:counter=43.000000
[0.000500] Notify[8000008]:SignalVector:binary_channel
[0.000500] Notify[8000008]:SignalVector:binary_channel:Signal:message=len(12)
...
scalar_channel:42:0:0::ModelExit
scalar_channel:42:0:0::ModelExit
```
</details>


## Go Package

The Trace tool implements a Visitor API which can be used to implement custom
trace commands. An example of a custom Visitor is as follows:

```go
package count

import "github.com/boschglobal/dse.modelc/extra/tools/trace/pkg/trace"

type CountVisitor struct {
    msgCount    uint32
    notifyCount uint32
}

func (c *CountVisitor) VisitChannelMsg(cm trace.ChannelMsg) {
    c.msgCount += 1
}

func (c *CountVisitor) VisitNotifyMsg(nm trace.NotifyMsg) {
    c.msgCount += 1
    c.notifyCount += 1
}
```
