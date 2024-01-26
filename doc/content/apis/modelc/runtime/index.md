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
}
```

### ModelChannelDesc

```c
typedef struct ModelChannelDesc {
    const char* name;
    const char* function_name;
    const char** signal_names;
    int signal_count;
    int propagator_source_channel;
    int propagator_target_channel;
    double* vector_double;
    void** vector_binary;
    int* vector_binary_size;
    int* vector_binary_buffer_size;
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
}
```

## Functions

### modelc_step

Execute a simulation step with the provided step size for all model
functions of the given model instance.

#### Parameters

model_instance : ModelInstanceSpec (pointer to)
    The model instance, which holds references to the registered channels
and model functions. step_size : double The duration simulation step to be
performed (in seconds).

#### Returns

    0 : Success.
    +ve/-ve : Failure, inspect `errno` for the failing condition.



