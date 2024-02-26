// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_GATEWAY_H_
#define DSE_MODELC_GATEWAY_H_

#include <stdint.h>
#include <errno.h>
#include <dse/platform.h>
#include <dse/modelc/model.h>


/**
Gateway Model
=============

When implemented, a Gateway Model makes it possible for a foreign Simulation
Environment to connect with a Dynamic Simulation Environment. The two
simulation environments can then exchange signals and maintain synchronisation.

Component Diagram
-----------------
<div hidden>

```
@startuml gateway-model

title Gateway Model

node "Dynamic Simulation Environment" {
        component "Model" as m1
        component "Model" as m2
        interface "SimBus" as SBif
        m1 -left-> SBif
        m2 -right-> SBif
}
package "Gateway Model" {
        component "ModelC Lib" as ModelC
        component "Model"
}

SBif <-down- ModelC
Model -up-> ModelC :model_gw_setup()
Model -up-> ModelC :model_gw_sync()
Model -up-> ModelC :model_gw_exit()

center footer Dynamic Simulation Environment

@enduml
```

</div>

![](gateway-model.png)


Example
-------

{{< readfile file="../examples/gateway.c" code="true" lang="c" >}}

*/
#define __GATEWAY_ERROR_OFFSET (__MODELC_ERROR_OFFSET + 1000)
#define E_GATEWAYBEHIND        (__GATEWAY_ERROR_OFFSET + 1)


typedef struct ModelGatewayDesc {
    SimulationSpec*    sim;
    ModelInstanceSpec* mi;
    SignalVector*      sv; /* Null terminated list. */
    /* References to allocated memory. */
    const char**       argv;
    char*              name_arg;
    /* Sync epsilon (i.e. clock tolerance). */
    double             clock_epsilon;
} ModelGatewayDesc;


/* gateway.c - Gateway Model Interface. */
DLL_PUBLIC int model_gw_setup(ModelGatewayDesc* gw, const char* name,
    const char** yaml_files, int log_level, double step_size, double end_time);
DLL_PUBLIC int model_gw_sync(ModelGatewayDesc* gw, double model_time);
DLL_PUBLIC int model_gw_exit(ModelGatewayDesc* gw);


#endif  // DSE_MODELC_GATEWAY_H_
