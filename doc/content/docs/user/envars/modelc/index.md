---
title: "ModelC Environment Variables"
linkTitle: "ModelC"
weight: 600
tags:
- Envar
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc"
---

## Tool Specific Environment Variables

### ModelC

| Variable           | CLI Option    | Default |
| ------------------ | ------------- | ------- |
| `NCODEC_TRACE_{bus}_{bus_id}` | _N/A_ | _None_ |
| `NCODEC_TRACE_PDU_{swc_id}` | _N/A_   | _None_ |
| `SIMBUS_LOGLEVEL`  | `--logger`    | `4` (LOG_NOTICE) |
| `SIMBUS_TRANSPORT` | `--transport` | `redispubsub` |
| `SIMBUS_URI`       | `--uri`       | `redis://localhost:6379` |


### SimBus

| Variable           | CLI Option    | Default |
| ------------------ | ------------- | ------- |
| `SIMBUS_LOGLEVEL`  | `--logger`    | `4` (LOG_NOTICE) |
| `SIMBUS_TRACEFILE` | _N/A_         | _None_ (trace disabled, path to trace file) |
| `SIMBUS_TRANSPORT` | `--transport` | `redispubsub` |
| `SIMBUS_URI`       | `--uri`       | `redis://localhost:6379` |


### Examples

#### Benchmark

| Variable           | CLI Option    | Default |
| ------------------ | ------------- | ------- |
| `TAG`              | _N/A_         | _None_ (a log prefix, configure as a string) |


#### Block

| Variable           | CLI Option    | Default |
| ------------------ | ------------- | ------- |
| `MODEL_COUNT`      | _N/A_         | `1`     |
| `MODEL_ID`         | _N/A_         | `1`     |


#### CSV

| Variable           | CLI Option    | Default |
| ------------------ | ------------- | ------- |
| `CSV_FILE`         | _N/A_         | _None_ (path to CSV file) |
