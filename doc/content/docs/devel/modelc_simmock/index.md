---
title: "Testing Models with SimMock"
linkTitle: "Testing Models"
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


### Example Test Files

<details>
<summary>tests/cmocka/network/test_network.c</summary>

{{< readfile file="examples/tests/cmocka/network/test_network.c" code="true" lang="c" >}}

</details>



## References and Further Reading

* [Testing with CMocka]({{< relref "docs/devel/modelc_cmocka/index.md" >}})
