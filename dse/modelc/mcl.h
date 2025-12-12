// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_MCL_H_
#define DSE_MODELC_MCL_H_

#include <stdint.h>
#include <dse/platform.h>
#include <dse/clib/data/marshal.h>
#include <dse/modelc/model.h>


#ifndef DLL_PUBLIC
#define DLL_PUBLIC __attribute__((visibility("default")))
#endif
#ifndef DLL_PRIVATE
#define DLL_PRIVATE __attribute__((visibility("hidden")))
#endif


/**
Model Compatibility Library API
===============================

A Model Compatibility Library provides an interface for supporting 3rd-party
model interfaces in a DSE Simulation.


Component Diagram
-----------------
<div hidden>

```
@startuml mcl-interface

skinparam nodesep 55
skinparam ranksep 40

title MCL Interface

component "Model" as m1
component "Model" as m2
interface "SimBus" as SBif
m1 -left-> SBif
m2 -right-> SBif


package "MCL" {
        component "Runtime" as ModelC
        component "MCL" as Mcl
        interface "ModelVTable" as MVt
        interface "MclVTable" as MclVt
        component "MCL Lib" as MclLib
}


MclLib -up- MclVt
MclLib -up- MVt

SBif <-down- ModelC
MVt )-up- ModelC
MclVt )-up- Mcl

component "Model" as MclModel
interface "Model I/F" as ModelIf
MclModel -up- ModelIf
ModelIf )-up- MclLib

center footer Dynamic Simulation Environment

@enduml
```

</div>

![](mcl-interface.png)

*/


typedef struct MclDesc MclDesc;


typedef MclDesc* (*MclCreate)(ModelDesc* m);
typedef void (*MclDestroy)(MclDesc* m);

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

    /* Reserved (space for 2 function pointers). */
    void* __reserved__[2];
} MclVTable;


typedef struct MclDesc {
    ModelDesc   model;
    /* Extensions to base ModelDesc type. */
    const char* adapter;
    const char* version;
    MclVTable   vtable;
    /* Model Time Calculation. */
    double      step_size;
    double      model_time;
    double      model_time_correction;
    /* References for Signal Vector marshalling. */
    struct {
        size_t       count;
        const char** signal;
        union {
            double* scalar;
            void**  binary;
        };
        uint32_t*    binary_len;
        MarshalKind* kind;
    } source;
    MarshalSignalMap* msm;  // NULL terminated list

    /* Reserved. */
    uint64_t __reserved__[4];
} MclDesc;


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
