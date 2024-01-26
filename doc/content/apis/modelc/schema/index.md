---
title: Schema API Reference
linkTitle: Schema
---
## Schema API


The Schema API provides a number of functions for parsing the YAML configuration
documents which represent (some of) the Schemas of the Model C Library.



## Typedefs

### SchemaLabel

```c
typedef struct SchemaLabel {
    const char* name;
    const char* value;
}
```

### SchemaObject

```c
typedef struct SchemaObject {
    const char* kind;
    const char* name;
    void* doc;
    void* data;
}
```

### SchemaObjectSelector

```c
typedef struct SchemaObjectSelector {
    const char* kind;
    const char* name;
    SchemaLabel* labels;
    int labels_len;
    void* data;
}
```

### SchemaSignalObject

```c
typedef struct SchemaSignalObject {
    const char* signal;
    void* data;
}
```

## Functions

### schema_build_channel_selector

Builds a channel selector object based on the provided Model Instance and
Channel Spec. The caller should free the object by calling
`schema_release_channel_selector()`.

The returned SchemaObjectSelector can be used when calling
schema_object_search() to search for schema objects.

> Note: A channel selector will not match on metadata/name.

#### Parameters

model_instance (ModelInstanceSpec*)
: The Model Instance, which holds references to the various schema objects
  that will searched by the generated selector.

channel (ChannelSpec*)
: A channel spec object.

kind (const char*)
: The kind of schema object to select.

#### Returns

SchemaObjectSelector (pointer)
: The complete selector object.

NULL
: A selector object could not be created. This return value does not represent
  an error condition. The caller will determine if this condition represents
  and error (typically a configuration error).
 


### schema_object_enumerator

Enumerate over all child objects of a schema list object. Each child object
is marshalled via the generator function and returned to the caller.

When index exceeds the length of the schema list object the function
returns NULL and the enumeration is complete.

#### Parameters

model_instance (ModelInstanceSpec*)
: The Model Instance, which holds references to the various schema objects
  which will be enumerated over..

object (SchemaObject*)
: The schema list object to enumerate over.

path (const char*)
: Enumerate objects located at this path, relative from the `object`.

index (uint32_t*)
: Maintains the enumerator postion between calls. Set to 0 to begin a new
  schema object enumeration (i.e. from the first object in the list).

generator (SchemaObjectGenerator)
: A generator function which creates the required schema object.

#### Returns

void*
: Pointer to the generated object created by the `generator` function. The
  caller must free this object.

NULL
: The enumeration is complete.
 


### schema_object_search

Search the collection of schema objects according to the selector, and
call the handler function for each matching object. Schema objects are
searched in the order they were parsed (i.e. listed order at the CLI).

#### Example


{{< readfile file="../examples/schema_object_search.c" code="true" lang="c" >}}

#### Parameters

model_instance (ModelInstanceSpec*)
: The Model Instance, which holds references to the various schema objects
  which will be searched.

selector (SchemaObjectSelector*)
: A selector object, schema objects are matched as follows:
   * kind - matches the object kind (if NULL matches _all_ object kinds).
   * name - matches the object name (if NULL matches _all_ object names).
   * labels[] - matches _all_ labels of the object (i.e. _AND_).

handler (SchemaMatchHandler)
: A handler function which is called for each matching schema object. The
  handler can control the search continuation by returning as follows:
   * 0 - the search should continue until no more matches are found.
   * +ve -  the search should stop and return 0 (indicating success).
   * -ve - the search should abort, set errno with a value to indicate
     the failing condition, and return +ve (indicating failure).

#### Returns

0
: The schema search successfully completed.

+ve
: Failure, inspect errno for an indicator of the failing condition.



### schema_release_selector

Release any allocated memory in a SchemaObjectSelector object.

#### Parameters

selector (SchemaObjectSelector*)
: A selector object, created by calling `schema_build_channel_selector()`.



### schema_signal_object_generator

Generate a schema signal object.

#### Parameters

model_instance (ModelInstanceSpec*)
: The Model Instance, which holds references to the various schema objects
  which will be searched.

data (void*)
: The YAML node to generate from.

#### Returns

void*
: Pointer to the generated schema signal object. The caller must free
  this object.

NULL
: The object cannot be generated.
 


