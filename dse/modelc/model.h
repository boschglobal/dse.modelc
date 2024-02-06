// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_MODEL_H_
#define DSE_MODELC_MODEL_H_

#include <stdint.h>
#include <stdbool.h>


#ifndef DLL_PUBLIC
#if defined _WIN32 || defined __CYGWIN__
#ifdef DLL_BUILD
#define DLL_PUBLIC __declspec(dllexport)
#else
#define DLL_PUBLIC __declspec(dllimport)
#endif
#else
#define DLL_PUBLIC __attribute__((visibility("default")))
#endif
#endif
#ifndef DLL_PRIVATE
#if defined _WIN32 || defined __CYGWIN__
#define DLL_PRIVATE
#else
#define DLL_PRIVATE __attribute__((visibility("hidden")))
#endif
#endif


#define __MODELC_ERROR_OFFSET   (2000)
#define MODEL_DEFAULT_STEP_SIZE 0.0005
#define MODEL_CREATE_FUNC_NAME  "model_create"
#define MODEL_STEP_FUNC_NAME    "model_step"
#define MODEL_DESTROY_FUNC_NAME "model_destroy"


typedef struct SimulationSpec    SimulationSpec;
typedef struct ModelInstanceSpec ModelInstanceSpec;
typedef struct SignalVector      SignalVector;
typedef struct ModelDesc         ModelDesc;
typedef struct ModelSignalIndex  ModelSignalIndex;


/**
Model API
=========

The Model API allows model developers and integrators to implement models which
can be connected to a Simulation Bus.
Models are able to exchange signals with other models via this connection to
a Simulation Bus.
A runtime environment, such as the ModelC Runtime/Importer, will load the
model and also manages the connection with the Simulation Bus.

The Model API provides two simple interfaces which facilitate the development
of models; the Model Interface which is concerned with the model lifecycle; and
the Signal Interface which facilitates signal exchange.


Model Interface
---------------

The Model Interface provides the necessary types, methods and objects required
for implementing a model. Such a model can easily participate in a simulation
by being connecting to a Simulation Bus (using the ModelC Importer) and then
exchanging signals with other models in that simulation by using the
provided SignalVector objects (which represent those signals).

Additionally, model implementers may extend or modify the Model Interface
to support more complex integrations.


Signal Vector Interface
-----------------------

Models exchange signals via the Simulation Bus using a Signal Vector. Signal
Vectors represent a logical grouping of signals (e.g. a collection of signals
belonging to an ECU interface or bus). They are defined by a `SignalGroup`
schema kind and may be configured to represent either either scalar
(double, int, bool) or binary values.


Component Diagram
-----------------
<div hidden>

```
@startuml model-interface

skinparam nodesep 55
skinparam ranksep 40

title Model Interface

component "Model" as m1
component "Model" as m2
interface "SimBus" as SBif
m1 -left-> SBif
m2 -right-> SBif

package "Model" {
        component "Runtime" as ModelC
        interface "ModelVTable" as Mvt
        component "Model" as Mdl
}

SBif <-down- ModelC
Mdl -up- Mvt
Mvt )-up- ModelC

center footer Dynamic Simulation Environment

@enduml
```

</div>

![](model-interface.png)


Example (Model Interface)
-------

{{< readfile file="../examples/model_interface.c" code="true" lang="c" >}}


Example (Signal Vector Interface)
-------

{{< readfile file="../examples/signalvector_interface.c" code="true" lang="c" >}}

*/


/* Model Interface. */

typedef ModelDesc* (*ModelCreate)(ModelDesc* m);
typedef int (*ModelStep)(ModelDesc* m, double* model_time, double stop_time);
typedef void (*ModelDestroy)(ModelDesc* m);
typedef ModelSignalIndex (*ModelIndex)(ModelDesc* m, const char* vname,
    const char* sname);


typedef struct ModelVTable {
    ModelCreate  create;
    ModelStep    step;
    ModelDestroy destroy;
    ModelIndex   index;
} ModelVTable;


typedef struct ModelDesc {
    ModelVTable        vtable;
    ModelIndex         index;
    SimulationSpec*    sim;
    ModelInstanceSpec* mi;
    SignalVector*      sv;
} ModelDesc;


typedef struct ModelSignalIndex {
    /* References, only set if index is valid. */
    SignalVector* sv;
    double*       scalar;
    void**        binary;
    /* Indexes to the SignalVector object. */
    uint32_t      vector;
    uint32_t      signal;
} ModelSignalIndex;


/* Implemented by Model. */
DLL_PUBLIC ModelDesc* model_create(ModelDesc* m);
DLL_PUBLIC int model_step(ModelDesc* m, double* model_time, double stop_time);
DLL_PUBLIC void model_destroy(ModelDesc* m);


/* Provided by ModelC. */
DLL_PUBLIC ModelSignalIndex model_index_(
    ModelDesc* model, const char* vname, const char* sname);
DLL_PUBLIC const char* model_annotation(ModelDesc* m, const char* name);
DLL_PUBLIC const char* model_instance_annotation(ModelDesc* m, const char* name);


/* Signal Interface. */

typedef int (*BinarySignalAppendFunc)(
    SignalVector* sv, uint32_t index, void* data, uint32_t len);
typedef int (*BinarySignalResetFunc)(SignalVector* sv, uint32_t index);
typedef int (*BinarySignalReleaseFunc)(SignalVector* sv, uint32_t index);
typedef void* (*BinarySignalCodecFunc)(SignalVector* sv, uint32_t index);
typedef const char* (*SignalAnnotationGetFunc)(
    SignalVector* sv, uint32_t index, const char* name);


typedef struct SignalVectorVTable {
    BinarySignalAppendFunc  append;
    BinarySignalResetFunc   reset;
    BinarySignalReleaseFunc release;
    SignalAnnotationGetFunc annotation;
    BinarySignalCodecFunc   codec;
} SignalVectorVTable;


typedef struct SignalVector {
    const char*  name;
    const char*  alias;
    const char*  function_name;
    bool         is_binary;
    /* Vector representation of Signals (each with _count_ elements). */
    uint32_t     count;
    const char** signal; /* Signal name. */
    union {              /* Signal value. */
        struct {
            double* scalar;
        };
        struct {
            void**       binary;
            uint32_t*    length;      /* Length of binary object. */
            uint32_t*    buffer_size; /* Size of allocated buffer. */
            const char** mime_type;
            void**       ncodec;      /* Network Codec objects. */
        };
    };
    /* Helper functions. */
    BinarySignalAppendFunc  append;
    BinarySignalResetFunc   reset;
    BinarySignalReleaseFunc release;
    SignalAnnotationGetFunc annotation;
    BinarySignalCodecFunc   codec;
    /* Reference data. */
    ModelInstanceSpec*      mi;
} SignalVector;


/* Provided by ModelC (virtual methods of SignalVectorVTable). */
DLL_PUBLIC int signal_append(SignalVector* sv, uint32_t index,
    void* data, uint32_t len);
DLL_PUBLIC int signal_reset(SignalVector* sv, uint32_t index);
DLL_PUBLIC int signal_release(SignalVector* sv, uint32_t index);
DLL_PUBLIC void* signal_codec(SignalVector* sv, uint32_t index);
DLL_PUBLIC const char* signal_annotation(SignalVector* sv, uint32_t index,
    const char* name);


#endif  // DSE_MODELC_MODEL_H_
