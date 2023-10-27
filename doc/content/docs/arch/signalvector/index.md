---
title: "Signal Vectors"
linkTitle: "Signals"
weight: 1020
tags:
  - ModelC
  - Architecture
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc"
---

## Synopsis

The Dynamic Simulation Environment (DSE) presents models with a simple vector interface for the exchange of signals. Those signals can be either:

- **scalar** : Internally represented as a 64bit storage container (double). These values are transparently exchanged between models, and those models may cast the values to other types (as required by the simulation designer).
- **binary** : Binary strings (i.e. including embedded NULL characters) can be exchanged between models, and with an associated _MIME type_, complex bus simulations can be realised.


An associated API can be used to access signal vectors:

```c
#include <dse/modelc/model.h>

int model_function(ModelInstanceSpec* mi)
{
    SignalVector* sv = model_sv_create(mi);
    while (sv && sv->name) {
        for (uint i = 0; i++; i < sv->count) {
            if (sv->is_binary) {
                sv->reset(sv, i);
                sv->append(sv, i, "hello world", 12);
                printf("%s = %f (%s)\n", sv->signal[i], sv->binary[i], sv->mime_type[i]);
            } else {
                printf("%s = %f\n", sv->signal[i], sv->scalar[i]);
            }
        }
        /* Next signal vector. */
        sv++;
    }
}
```



## Scalar Signal Vector

Scalar signal vectors are used by models to exchange signal values. Those values of a scalar signal vector are represented internally as 64bit storage container (double), and they are transparently exchanged between models. Models can cast these values to any scalar data type as required.

{{% alert title="Tip" color="info" %}} When casting scalar values, use a signal annotation to describe the type cast which should be used. That way, simulation integrators and other model developers will be aware of how the signal value should be interpreted. {{% /alert %}}


### API Methods

```c
/* Access signal annotations. */
const char* value = sv->annotation(sv, i, "name");
```


### Configuration

Scalar signal vectors are represented by a `SignalGroup` YAML document. Individual signals may have additional annotations which can be used to describe the behaviour or properties of the signal - those annotations may be interpreted by a model which uses those signals.


**Scalar Signal Vector :**
```yaml
kind: SignalGroup
metadata:
  name: scalar
spec:
  signals:
    - signal: foo
    - signal: bar
      annotations:
        type-cast: int32
```



## Binary Signal Vector

Binary signal vectors are used by models to exchange binary data, the effect of which is to create binary data streams between models. When associated with a _MIME type_, complex communication protocols can be realised (e.g. CAN Bus simulation). When several models exchange binary data values for the same signal, each of those values will be consolidated into a single value which represents a binary stream, and that consolidated value is exchanged between all models.

Because the binary signals may represent variable length data objects, there are a collection of API Methods available to reliably interface with this vector type.


### API Methods

```c
/* Work with a binary signal. */
sv->reset(sv, i);  /* Reset the binary signal (i.e. length = 0). */
sv->append(sv, i, data, length);  /* Append data to the signal. */
sv->release(sv, i);  /* Free the memory buffer of a binary signal. */

/* Access signal annotations. */
const char* value = sv->annotation(sv, i, "name");
```

{{% alert title="Tip" color="info" %}} Generally calling `sv->release()` is not necessary and, if avoided, will result in less memory allocation overhead during model runtime. {{% /alert %}}


### Configuration

Binary signal vectors are represented by a `SignalGroup` YAML document with a ***metadata annotation*** **`vector_type: binary`**. Each signal of a binary signal vector should also have an annotation `mime_type` which describes the binary data. The default value provided by the Signal Vector API is `application/octet-stream` (which, from a system integration perspective, is almost totally useless).

Individual signals may have additional annotations which can be used to describe the behaviour or properties of the signal - those annotations may be interpreted by a model which uses those signals.


**Binary Signal Vector :**
```yaml
kind: SignalGroup
metadata:
  name: binary
  annotations:
    vector_type: binary
spec:
  signals:
    - signal: foo
      annotations:
        mime_type: 'application/octet-stream; interface=parameter; file=calibration.csv'
    - signal: bar
      annotations:
        mime_type: 'application/x-automotive-bus; interface=stream; type=frame; schema=fbs'
```



## References

* [dse/modelc/model.h](https://github.com/boschglobal/dse.modelc/blob/main/dse/modelc/model.h) - definition of Signal Vector API.
* [SignalGroup](https://github.com/boschglobal/dse.schemas/blob/main/schemas/yaml/SignalGroup.yaml) schema definition.
* DSE Model C examples:
    * [Binary Model](https://github.com/boschglobal/dse.modelc/tree/main/dse/modelc/examples/binary)
    * [Gateway Model](https://github.com/boschglobal/dse.modelc/tree/main/dse/modelc/examples/gateway)
* [Frame Stream Interface (Automotive Bus)](https://github.com/boschglobal/automotive-bus-schema/blob/main/schemas/stream/frame.fbs) with [sample](https://github.com/boschglobal/automotive-bus-schema/tree/main/examples/streams) code.
