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

| Variable                      | CLI Option        | Default |
| ----------------------------- | ----------------- | ------- |
| `NCODEC_TRACE_LOG`            | _N/A_ | _None_    |
| `NCODEC_TRACE_{bus}_{bus_id}` | _N/A_ | _None_    |
| `NCODEC_TRACE_PDU_{swc_id}`   | _N/A_   | _None_  |
| `SIMBUS_LOGLEVEL`             | `--logger`        | `4` (LOG_NOTICE) |
| `SIMBUS_TRANSPORT`            | `--transport`     | `redispubsub` |
| `SIMBUS_URI`                  | `--uri`           | `redis://localhost:6379` |
| `SIMBUS_TIMEOUT`              | `--timeout`       | `60` (seconds) |


### SimBus

| Variable            | CLI Option    | Default |
| ------------------- | ------------- | ------- |
| `SIMBUS_LOGLEVEL`   | `--logger`    | `4` (LOG_NOTICE) |
| `SIMBUS_TRACE_FILE` | _N/A_         | _None_ (trace disabled, path to trace file) |
| `SIMBUS_TRACE_PORT` | _N/A_         | _None_ (trace disabled, UDP port for trace) |
| `SIMBUS_TRACE_UNIX` | _N/A_         | _None_ (trace disabled, path to socket, e.g. /tmp/simbus_trace.sock) |
| `SIMBUS_TRANSPORT`  | `--transport` | `redispubsub` |
| `SIMBUS_URI`        | `--uri`       | `redis://localhost:6379` |
| `SIMBUS_TIMEOUT`    | `--timeout`   | `60` (seconds) |


### Examples

#### Benchmark

| Variable           | CLI Option    | Default |
| ------------------ | ------------- | ------- |
| `MODEL_ID`         | _N/A_         | `1`     |
| `SIGNAL_CHANGE`    | _N/A_         | `5`     |
| `STARTUP_ANNO`     | _N/A_         | `1`     |
| `STARTUP_IDX`      | _N/A_         | `1`     |
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
| `CSV_LINE_MAXLEN`  | _N/A_         | `1024` |
