---
title: "Trace - Simulation Trace Tool"
linkTitle: "Trace"
weight: 10
tags:
- trace
- CLI
- NCodec
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc"
---


## Synopsis

Simulation trace tool.

```bash
# Convert a SimBus trace to CSV format:
$ trace convert --csv testdata/simbus.bin
...
Create measurement file: simbus.scalar.csv
$ cat simbus.scalar.csv
time,SIG_4,SIG_6,SIG_1,SIG_3,SIG_5,SIG_2
0.000500,0.0,0.0,1,3,0.0,2
0.006500,1,3,1,3,0.0,2
0.016500,1,3,1,3,2,2
0.020500,1,3,2,6,2,4
0.026500,1,6,2,6,4,4
0.031500,2,6,2,6,4,4

# Convert a NCodec trace to Vector ASC format:
$ trace convert --asc --name FR_1 --tx 5 ncodec.bin
...
Create measurement file: ncodec_pdu.FR_1.asc
$ cat  ncodec_pdu.FR_1.asc
date Tue May 19 02:50:18.743 pm 2026
base hex  timestamps absolute
internal events logged
Begin Triggerblock Tue May 19 02:50:18.743 pm 2026
        0.0000 Start of measurement
        0.0065  Fr  1 Tx  1   11  0  1  18 0x00 0x0000 0 0x00 00 01 93 08 03 00 00 00 00 00 00 00
        0.0165  Fr  1 Tx  3   11  0  1  18 0x00 0x0000 0 0x00 00 01 92 08 02 00 00 00 00 00 00 00
        0.0265  Fr  1 Tx  5   11  0  1  18 0x00 0x0000 0 0x00 00 01 92 08 04 00 00 00 00 00 00 00
        0.0315  Fr  1 Tx  6   11  0  1  18 0x00 0x0000 0 0x00 00 01 91 08 02 00 00 00 00 00 00 00
End Triggerblock
```


## Commands

```bash
$ trace --help
Trace tools for working with SimBus and NCodec trace files.

Usage:

        trace [--verbose] <command> [option] <trace file>

        trace convert [--csv, --asc] [--name <net name>] [--tx <ecu id>] <trace file>
        trace summary [--short, --long] <trace file>


        trace convert --csv simbus.bin
        trace convert --asc simbus.bin
        trace convert --asc --name FR_1 --tx 5 ncodec.bin

  summary
    Options:
      -long *flag.boolValue
          generate a long summary  (default: false)
      -short *flag.boolValue
          generate a short summary  (default: true)
  convert
    Options:
      -asc *flag.boolValue
          convert to ASC measurement format (network channels)  (default: false)
      -csv *flag.boolValue
          convert to CSV measurement format (scalar channels)  (default: false)
      -name *flag.stringValue
          measurement name (NCodec trace only)
      -tx *flag.uintValue
          Tx ECU identifier  (default: 0)
```

> __Note__: The `trace` tool is included in the Simer container image, where it is installed at `/usr/local/bin/trace`.
A Linux version can also be downloaded from the [ModelC releases](https://github.com/boschglobal/dse.modelc/releases) page.


### Convert → CSV

Convert scalar signals from a SimBus trace to a CSV file.
The output file is written as `<trace>.<channel>.csv`, where `channel` is defined in the simulation (and extracted from the trace).

**Example:**

```bash
$ simer path/to/simulation --env simbus:SIMBUS_TRACE_FILE=simbus.bin --endtime 0.100
$ trace convert --csv simbus.bin
$ cat simbus.scalar.csv
time,SIG_4,SIG_6,SIG_1,SIG_3,SIG_5,SIG_2
0.000500,0.0,0.0,1,3,0.0,2
0.006500,1,3,1,3,0.0,2
0.016500,1,3,1,3,2,2
0.020500,1,3,2,6,2,4
0.026500,1,6,2,6,4,4
0.031500,2,6,2,6,4,4
```


### Convert → ASC

Convert network messages from a NCodec or SimBus trace to a Vector ASC file.
The output file is written as `<trace>.<name>.asc`. For NCodec trace files, `<name>` must be specified with `--name`, otherwise the name of the binary signal contained in the SimBus trace is used.

**Example:**

```bash
$ simer path/to/simulation --env model_name:NCODEC_TRACE_FILE=ncodec.bin --endtime 0.100
$ trace convert --asc --name FR_1 --tx 5 ncodec.bin
$ cat ncodec.FR_1.asc
date Tue May 19 02:50:18.743 pm 2026
base hex  timestamps absolute
internal events logged
Begin Triggerblock Tue May 19 02:50:18.743 pm 2026
        0.0000 Start of measurement
        0.0065  Fr  1 Tx  1   11  0  1  18 0x00 0x0000 0 0x00 00 01 93 08 03 00 00 00 00 00 00 00
        0.0165  Fr  1 Tx  3   11  0  1  18 0x00 0x0000 0 0x00 00 01 92 08 02 00 00 00 00 00 00 00
        0.0265  Fr  1 Tx  5   11  0  1  18 0x00 0x0000 0 0x00 00 01 92 08 04 00 00 00 00 00 00 00
        0.0315  Fr  1 Tx  6   11  0  1  18 0x00 0x0000 0 0x00 00 01 91 08 02 00 00 00 00 00 00 00
End Triggerblock
```




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
