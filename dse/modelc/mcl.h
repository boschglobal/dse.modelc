// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_MCL_H_
#define DSE_MODELC_MCL_H_

#include <stdint.h>
#include <dse/platform.h>
#include <dse/clib/data/marshal.h>
#include <dse/modelc/model.h>


/**
Model Compatibility Library
===========================

A Model Compatibility Library provides an interface for operating foreign
models.


Component Diagram
-----------------
<div hidden>

```
@startuml mcl-component

title Model Compatibility Library

TODO

center footer Dynamic Simulation Environment

@enduml
```

</div>

![](mcl-component.png)


Example
-------

{{< readfile file="examples/mcl_api.c" code="true" lang="c" >}}

*/


typedef struct MclDesc MclDesc;


typedef MclDesc* (*MclCreate)(ModelDesc* m);

typedef int32_t (*MclLoad)(MclDesc* m);
typedef int32_t (*MclInit)(MclDesc* m);
typedef int32_t (*MclStep)(MclDesc* m, double* model_time, double end_time);
typedef int32_t (*MclMarshalOut)(MclDesc* m);
typedef int32_t (*MclMarshalIn)(MclDesc* m);
typedef int32_t (*MclUnload)(MclDesc* m);


typedef struct MclVTable {
    MclLoad       load;
    MclInit       init;
    MclStep       step;
    MclMarshalOut marshal_out;
    MclMarshalIn  marshal_in;
    MclUnload     unload;
} MclVTable;


typedef struct MclDesc {
    ModelDesc model;
    /* Extensions to base ModelDesc type. */
    const char* adapter;
    const char* version;
    MclVTable vtable;
    /* Model Time Calculation. */
    double step_size;
    double model_time;
    double model_time_correction;
    /* References for Signal Vector marshalling. */
    struct {
        size_t*       count;
        const char**  signal;
        double**      scalar;
    } source;
    MarshalSignalMap* msm; // NULL terminated list
} MclDesc;


/** MCL API */

/* Implemented by an MCL. */
DLL_PUBLIC MclDesc* mcl_create(ModelDesc* m);
DLL_PUBLIC void     mcl_destroy(MclDesc* m);

/* Implemented by ModelC (VTable wrappers). */
DLL_PUBLIC int32_t mcl_load(MclDesc* m);
DLL_PUBLIC int32_t mcl_init(MclDesc* m);
DLL_PUBLIC int32_t mcl_step(MclDesc* m, double end_time);
DLL_PUBLIC int32_t mcl_marshal_out(MclDesc* m);
DLL_PUBLIC int32_t mcl_marshal_in(MclDesc* m);
DLL_PUBLIC int32_t mcl_unload(MclDesc* m);


#endif  // DSE_MODELC_MCL_H_
