---
title: "Testing Models with SimMock"
linkTitle: "SimMock"
draft: false
tags:
- Developer
- Testing
- CMocka
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc"
---

## SimMock and CMocka Testing for Model Developers

The DSE ModelC Library includes a SimMock source code library which may be
used to develop integration tests for Models. With the SimMock library, a
Model Developer is able to easily develop CMocka Testcases which:

* Validate a model configuration and confirm the Models operation.
* Develop scenarios where expected signal exchange is verified.
* Inject network messages into the simulation or model (e.g. CAN frames) and read network messages.
* Check expected values for both scalars (Signals) and messages (Network).
* Stack several Model Instances within one SimMock and confirm their interoperation.


### ModelC Shared Library Stub

When testing models directly with the SimMock it may be necessary to use a stub
of the `modelc.so` library. The following integrations are required:

<summary>tests/cmocka/Makefile</summary>

```makefile
export LD_LIBRARY_PATH=$(shell pwd)/build/_out/lib
```

<summary>tests/cmocka/CMakeLists.txt</summary>

```cmake
# Target - ModelC Stub
# --------------------
add_library(modelc_stub SHARED
    ${DSE_MOCKS_SOURCE_DIR}/modelc_stub.c
)
set_target_properties(modelc_stub
    PROPERTIES
        OUTPUT_NAME modelc
)
install(TARGETS modelc_stub)
```


### Example Test Files

<details>
<summary>network/test_network.c</summary>

{{< readfile file="../../../examples/modelc/mocks/network/test_network.c" code="true" lang="c" >}}

</details>



## References and Further Reading

* [Testing with CMocka]({{< relref "docs/devel/testing/cmocka" >}})
