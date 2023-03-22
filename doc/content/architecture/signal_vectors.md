---
title: "Signal Vectors"
linkTitle: "Signal Vectors"
weight: 20
tags:
  - ModelC
  - Architecture
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc/content/architecture"
path_base_for_github_subdir: "content/docs/architecture/"
---

## Synopsis

The Dynamic Simulation Environment (DSE) presents Models with a simple vector interface for the exchange of signals. Those signals can be either **numeric** (float/integer) or **binary** values. The configuration of a Signal Vector is described with a SignalGroup YAML file and realised via the Model C Library types `ModelChannelDesc.vector_double` (numeric) and `ModelChannelDesc.vector_binary`.



## Numeric Signal Vector

Values of the numeric signal vector are represented internally, and at the interface, as Double-precision floating-point numbers with a 64 bit width. However, as no operations are internally performed (on these values), it is possible for models to cast any data type into these values.

> Tip: When casting data types use annotations, either at the Signal Group or individual Signal, to indicate the nature of the cast.


#### Configuration

{{< card-code header="**SignalGroup: Numeric Signal Vector**" lang="yaml" highlight="" >}}
kind: SignalGroup
metadata:
  name: signal_vector_name
spec:
  signals:
    - signal: numeric_value
{{< /card-code >}}



## Binary Signal Vector

Values of the binary signal vector represent binary data streams. Any single value may include one or more packets of binary data. The content of each individual packet is defined by the mime_type annotation of each particular signal.

Because the binary signals may represent variable length data objects, there are a collection of API Methods available to reliably interface with this vector type.


#### Configuration

The necessary configuration items to define a binary signal vector are highlighted in the following example configuration. A `mime_type` should be configured for each signal of the binary signal vector.

{{< card-code header="**SignalGroup: Binary Signal Vector**" lang="yaml" highlight="hl_Lines=5-6 10-11" >}}
kind: SignalGroup
metadata:
  name: signal_vector_name
  annotations:
    vector_type: binary
spec:
  signals:
    - signal: binary_data
      annotations:
        mime_type: 'application/octet-stream'
{{< /card-code >}}


#### Model C Library Functions

> TODO: reference the Model C library functions for working with binary signal vectors. New API.


#### Example MIME Types and Schemas

* [Frame Stream Interface (Automotive Bus)](https://github.com/boschglobal/automotive-bus-schema/blob/main/schemas/stream/frame.fbs) with (sample)[https://github.com/boschglobal/automotive-bus-schema/tree/main/examples/streams] code.


#### Alternative Integrations

> TODO: based on Telsa ECU integration (i.e. no simbus, vector interface).
