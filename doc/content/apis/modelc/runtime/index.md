---
title: Runtime API Reference
linkTitle: Runtime
---
## Runtime API


The Runtime API provides methods for implementing a model Runtime/Importer
which can be used to load, configure and execute a model.




## Typedefs

### ChannelSpec

```c
typedef struct ChannelSpec {
    const char* name;
    const char* alias;
    void* private;
}
```

### ModelCArguments

```c
typedef struct ModelCArguments {
    const char* transport;
    char* uri;
    const char* host;
    int port;
    double timeout;
    int log_level;
    double step_size;
    double end_time;
    int uid;
    const char* name;
    const char* file;
    const char* path;
    void* yaml_doc_list;
    int timeout_set_by_cli;
    int log_level_set_by_cli;
    int steps;
    const char* sim_path;
    int [4] __reserved__;
}
```

### ModelDefinitionSpec

```c
typedef struct ModelDefinitionSpec {
    const char* name;
    const char* path;
    const char* file;
    char* full_path;
    void* doc;
    void* channels;
}
```

### ModelInstanceSpec

```c
typedef struct ModelInstanceSpec {
    int uid;
    char* name;
    int* model_desc;
    ModelDefinitionSpec model_definition;
    void* spec;
    void* yaml_doc_list;
    void* private;
    int [8] __reserved__;
}
```

### RuntimeModelDesc

```c
typedef struct RuntimeModelDesc {
    int model;
    struct {
        const char* runtime_model;
        char* sim_path;
        const char* model_name;
        const char* simulation_yaml;
        int argc;
        char** argv;
        void* doc_list;
        int log_level;
        double step_size;
        double end_time;
        double step_time_correction;
        int binary_signals_reset;
    } runtime;
    int [8] __reserved__;
}
```

### SimulationSpec

```c
typedef struct SimulationSpec {
    const char* transport;
    char* uri;
    int uid;
    double timeout;
    double step_size;
    double end_time;
    ModelInstanceSpec* instance_list;
    const char* sim_path;
    int mode_loopback;
    int sequential_cosim;
    int [4] __reserved__;
}
```

## Functions

### modelc_destroy

Bypass the controller and call model_destroy() directly.

#### Parameters

model_instance : ModelInstanceSpec (pointer to)
    The model instance, which holds references to the registered channels
and model functions. step_size : double The duration simulation step to be
performed (in seconds).



### modelc_step

Execute a simulation step with the provided step size for all model
functions of the given model instance.

The AdapterModel properties are normally set from a Start Message, however
when the SimBus is mocked (or not present) then the `stop_time` needs to be
set. `model_time` is set in the call to `step_model()`.

#### Parameters

model_instance : ModelInstanceSpec (pointer to)
    The model instance, which holds references to the registered channels
and model functions. step_size : double The duration simulation step to be
performed (in seconds).

#### Returns

    0 : Success.
    +ve/-ve : Failure, inspect `errno` for the failing condition.



