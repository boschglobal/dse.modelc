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

- **scalar** : Internally represented as a 64bit storage container (double). These values are transparently exchanged between models. Models may cast/convert these scalar values to other types as required.
- **binary** : Binary strings, which may container embedded NULL values, can be exchanged between models. Additionally a binary signal may be annotated with a _MIME type_ which describes the content of a binary signal.

{{% alert title="Tip" color="info" %}} Binary signals with an associated MIME type may be supported by a [Network Codec]({{< relref "docs/devel/modelc_ncodec/index.md" >}}) which has an API that simplifies interactions with binary data and can be used to realise network simulations (e.g. CAN bus messaging). {{% /alert %}}


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

{{% alert title="Tip" color="info" %}} Using a Network Codec simplifies operations with binary signals. {{% /alert %}}


### Configuration

Binary signal vectors are represented by a `SignalGroup` YAML document with a **metadata annotation** `vector_type: binary`. Each signal of a binary signal vector should also have an annotation `mime_type` which describes the binary data. The default value provided by the Signal Vector API is `application/octet-stream` (which, from a system integration perspective, is almost totally useless).

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



## Transformations

A scalar signal may be associated with a transformation by the `transform` node located alongside the `signal` node in a `SignalGroup` definition. The following transformations are supported:

| Transform | Description |
| --------- | ----------- |
| `linear`  | S~model~ = S~vector~ * `factor` + `offset` |


When signals are set by a model, all defined transformations are applied in the reverse direction _before_ those signal values are exchanged with other models in a simulation. Therefore a transformed signal value is only observable by a model which is associated with such a signal definition.


### Configuration

Transformations are configured for individual signals in a `SignalGroup` YAML document.

**Scalar Signal Vector with Transformations :**
```yaml
kind: SignalGroup
metadata:
  name: scalar
spec:
  signals:
    - signal: foo
    - signal: bar
      transform:
        linear:
          factor: 20
          offset: -100
```



## API Examples

### Using the Model API

(_definied in [dse/modelc/model.h](https://github.com/boschglobal/dse.modelc/blob/main/dse/modelc/model.h)_)

Signal vectors are located on the `ModelDesc` object (`model->sv`) as a NTL (Null terminated list) and can be indexed with the `model->index` function of the same object.

```c
int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    // Find a signal using the index method.
    ModelSignalIndex counter = model->index(m, "data", "counter");
    if (counter.scalar == NULL) return -EINVAL;
    *(counter.scalar) += 1;

    // Find a signal vector using the index method.
    ModelSignalIndex data_sv = model->index(m, "data", NULL);
    if (data_sv.sv == NULL) return -EINVAL;

    ...
}
```

### Using the Runtime API

(_definied in [dse/modelc/runtime.h](https://github.com/boschglobal/dse.modelc/blob/main/dse/modelc/runtime.h)_)

The Runtime API can be used to access signal vectors and is of particular use to tool developers and integrators who need access to runtime objects.

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



## References

* [dse/modelc/model.h](https://github.com/boschglobal/dse.modelc/blob/main/dse/modelc/model.h) - definition of Signal Vector API.
* [SignalGroup](https://github.com/boschglobal/dse.schemas/blob/main/schemas/yaml/SignalGroup.yaml) schema definition.
* DSE Model C examples:
  * [Binary Model](https://github.com/boschglobal/dse.modelc/tree/main/dse/modelc/examples/binary)
  * [NCodec Model](https://github.com/boschglobal/dse.modelc/tree/main/dse/modelc/examples/ncodec)
  * [Transform Model](https://github.com/boschglobal/dse.modelc/tree/main/dse/modelc/examples/transform)
* Binary Codecs:
  * [Network Codec]({{< relref "docs/devel/ncodec" >}}) from DSE Model C which implements a stream interface to the Network Codec API by using binary signals and associated MIME type annotations.
  * [Network Codec API](https://github.com/boschglobal/dse.standards/tree/main/dse/ncodec) generalised codec library with an example binary stream implementation.
