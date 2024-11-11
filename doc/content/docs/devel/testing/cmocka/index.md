---
title: "Testing with CMocka"
linkTitle: "CMocka"
draft: false
tags:
- Developer
- Testing
- CMocka
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc"
---

## CMocka Testing for C Code

The DSE Projects use CMocka for both Unit and Integration testing. This
document introduces the typical CMocka Test Project and explains the testing
features which are most often used when developing.



## Code Layout and Test Organisation


```text
L- tests/cmocka       Directory containing CMocka tests.
  L- target           An individual test target (single test executable).
    L- __tests__.c    Entry point for the test target (runs test groups).
    L- test_foo.c     Contains a test group (e.g. related tests).
  L- CMakeLists.txt   Make definitions for all test targets.
  L- Makefile         CMake automation.
L- Makefile           High-level build automation.
```


### Example Test Files

<details>
<summary>tests/cmocka/target/__test__.c</summary>

{{< readfile file="examples/tests/cmocka/target/__test__.c" code="true" lang="c" >}}

</details>
<br />
<details>
<summary>tests/cmocka/target/test_foo.c</summary>

{{< readfile file="examples/tests/cmocka/target/test_foo.c" code="true" lang="c" >}}

</details>
<br />
<details>
<summary>tests/cmocka/CMakeLists.txt</summary>

{{< readfile file="examples/tests/cmocka/CMakeLists.txt" code="true" lang="cmake" >}}

</details>
<br />
<details>
<summary>tests/cmocka/Makefile</summary>

{{< readfile file="examples/tests/cmocka/Makefile" code="true" lang="make" >}}

</details>
<br />
<details>
<summary>Makefile</summary>

{{< readfile file="examples/Makefile" code="true" lang="make" >}}

</details>


## Testing Features and Integrations

### DSE Testing Headers

The DSE C Lib includes a header file [dse/testing.h](https://github.com/boschglobal/dse.clib/blob/main/dse/testing.h)
which can be used to include the required set of CMocka headers and other testing related adaptations. The functionality
of the header is enabled with build defines as follows:

* `CMOCKA_TESTING` - enables the set of CMocka include files. Typically used in CMocka test projects.

* `UNIT_TESTING` - enables the set of CMocka include files __and__ also enables the extended memory testing functionality in CMocka.

When these defines are not used (i.e. during testing), the [dse/testing.h](https://github.com/boschglobal/dse.clib/blob/main/dse/testing.h) header file has no effect. It can be safely included by any source code.

> __Tip :__ Valgrind is an easier form of memory testing than that offered by CMocka. In general, CMocka memory testing
  requires instrumentation of all code included in a test target (via dse/testing.h). This becomes
  troublesome when integrating external libraries as the CMocka techniques do not work well when _all_
  code is not similarly instrumented (as is the case with external libraries). In short, Valgrind is easier.


### Valgrind Memory Checks

Valgrind memory checks can be enabled within the build environments by setting
the environment variable `GDB_CMD` before running a test target.

```bash
$ export GDB_CMD="valgrind -q --leak-check=full --track-origins=yes --error-exitcode=808"
$ make test
...
```

This is the default setting for many DSE Projects.


### GDB Based Debugging

Runtime exceptions (e.g. segmentation faults) can be quickly debugged by enabling
GDB within the build environments by setting the environment variable `GDB_CMD` before running a test target.

```bash
$ export GDB_CMD="gdb -q -ex='set confirm on' -ex=run -ex=quit"
$ make test
...
```

More information about using GDB is available at [Model C Debug Techniques]({{< relref "docs/devel/debug/modelc" >}}).



## Testing Techniques


### Data Driven Tests

An especially powerful technique for writing tests which uses tabular data to
execute and test a variety of conditions. A data driven test typically has two
parts:

1. The table (an array) containing data which will be fed, row by row, into the
   function being tested. Each row of the table will contain both arguments and
   expected responses (or other evaluation criteria).

2. The evaluation which, for each row of the table, will call the functions being
   tested and evaluate the results.


#### Example Data Driven Test

<details>
<summary>Example data driven test.</summary>

```c
#include <dse/testing.h>

typedef struct TestRow {
    int value;
    int result;
}

void test_foo__data_driven(void** state)
{
    Mock* mock = *state;
    TestRow tests[] =  {
        { .value = 5, .result = 10 },
        { .value = 8, .result = 16 },
        { .value = 2, .result = 4 },
    };

    for (size_t i = 0; i < ARRAY_SIZE(tests); i++) {
        int result = double(tests[i].value);
        assert_int_equal(result, tests[i].result);
    }
}
```

</details>


### Integration Testing

Integration testing is possible by creating a test which directly links against the
DSE Model C Library and uses its "MStep" API to load, configure and step a model. With
this technique it is possible to verify the following:

* Configuration files are correctly parsed.
* Model configuration and initialisation.
* Model operation, externally observed via changes to signals (i.e. Signal Vectors).
* Detection of memory leaks (using Valgrind).


#### Example MStep Test

<details>
<summary>Example MStep test loading and stepping a model.</summary>

```c
#include <dlfcn.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/modelc/model.h>
#include <dse/restbus/restbus.h>

void test_mstep(void** state)
{
    char* argv[] = {
        (char*)"test_mstep",
        (char*)"--name=stub_inst",
        (char*)"--logger=5",  // QUIET
        (char*)"examples/stub/data/stack.yaml",
        (char*)"../../../../tests/cmocka/mstep/model_mstep.yaml",
        (char*)"../../../../tests/cmocka/mstep/restbus_mstep.yaml",
    };
    int                argc = ARRAY_SIZE(argv);
    int                rc;
    ModelCArguments    args;
    SimulationSpec     sim;
    ModelInstanceSpec* mi;

    /* Setup the ModelC interfaces. */
    modelc_set_default_args(&args, "test", 0.0005, 0.0010);
    modelc_parse_arguments(&args, argc, argv, "MStep Test");
    rc = modelc_configure(&args, &sim);
    assert_int_equal(rc, 0);
    mi = modelc_get_model_instance(&sim, args.name);
    assert_non_null(mi);

    /* Directly load the Restbus Model. */
    void* handle =
        dlopen(mi->model_definition.full_path, RTLD_NOW | RTLD_LOCAL);
    assert_non_null(handle);
    ModelSetupHandler model_setup_func = dlsym(handle, MODEL_SETUP_FUNC_STR);
    ModelExitHandler  model_exit_func = dlsym(handle, MODEL_EXIT_FUNC_STR);
    assert_non_null(model_setup_func);
    assert_non_null(model_exit_func);

    /* Call the Model Setup. */
    rc = model_setup_func(mi);
    assert_int_equal(rc, 0);
    SignalVector* sv = model_sv_create(mi);

    /* Locate the restbus and network vectors. */
    SignalVector* sv_restbus = NULL;
    SignalVector* sv_network = NULL;
    while (sv && sv->name) {
        if (strcmp(sv->name, "restbus") == 0) sv_restbus = sv;
        if (strcmp(sv->name, "network") == 0) sv_network = sv;
        /* Next signal vector. */
        sv++;
    }
    assert_non_null(sv_restbus);
    assert_string_equal(sv_restbus->name, "restbus");
    assert_int_equal(sv_restbus->count, 3);
    assert_non_null(sv_restbus->scalar);
    assert_non_null(sv_network);
    assert_string_equal(sv_network->name, "network");
    assert_int_equal(sv_network->count, 1);
    assert_non_null(sv_network->binary);
    assert_non_null(sv_network->length);
    assert_non_null(sv_network->buffer_size);

    /* Check the initial values. */
    assert_double_equal(sv_restbus->scalar[0], 1.0, 0.0);
    assert_double_equal(sv_restbus->scalar[1], 0.0, 0.0);
    assert_double_equal(sv_restbus->scalar[2], 265.0, 0.0);
    assert_null(sv_network->binary[0]);
    assert_int_equal(sv_network->length[0], 0);
    assert_int_equal(sv_network->buffer_size[0], 0);

    /* Step the model - ensure no can_tx based on setting initial values. */
    rc = modelc_step(mi, args.step_size);
    assert_int_equal(rc, 0);
    assert_double_equal(sv_restbus->scalar[0], 1.0, 0.0);
    assert_double_equal(sv_restbus->scalar[1], 0.0, 0.0);
    assert_double_equal(sv_restbus->scalar[2], 265.0, 0.0);
    assert_null(sv_network->binary[0]);
    assert_int_equal(sv_network->length[0], 0);
    assert_int_equal(sv_network->buffer_size[0], 0);
    sv_network->reset(sv_network, 0);

    /* Call the Model Exit. */
    rc = model_exit_func(mi);
    assert_int_equal(rc, 0);
}
```

</details>
<br />
<details>
<summary>CMake file (partial) with linking to DSE Model C Library.</summary>

```cmake
...

# External Project - DSE ModelC Library (for linking to mstep)
# -------------------------------------
set(MODELC_BINARY_DIR "$ENV{EXTERNAL_BUILD_DIR}/dse.modelc.lib")
find_library(MODELC_LIB
    NAMES
        libmodelc_bundled.a
    PATHS
        ${MODELC_BINARY_DIR}/lib
    REQUIRED
    NO_DEFAULT_PATH
)
add_library(modelc STATIC IMPORTED GLOBAL)
set_target_properties(modelc
    PROPERTIES
        IMPORTED_LOCATION "${MODELC_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${MODELC_BINARY_DIR}"
)

...

# Target - MSTEP
# --------------
add_executable(test_mstep

)
target_include_directories(test_mstep
    PRIVATE
        ./
)
target_compile_definitions(test_mstep
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_libraries(test_mstep
    PUBLIC
        -Wl,-Bstatic modelc -Wl,-Bdynamic ${CMAKE_DL_LIBS}
    PRIVATE
        cmocka
        dl
        m
)
install(TARGETS test_mstep)
```

</details>


### Mocking Functions

Mock functions are used to adjust (or mimic) the behaviour of real functions in
situations where the normal behaviour of a real function is not desired; either
because the real function might not operate normally (missing dependencies), or
when a particular behaviour needs to be injected (to test a related behaviour).
CMocka provides a stack based mocking system which makes it possible to implement
and inject mocked functional behaviours.

These mocking features are:

* `expect_value()` - indicate that the mock function should be called with this
  parameter set to the specified value. Useful when the mock function is called
  indirectly from the code being tested.

* `check_expected()` - the mock function checks that the provided parameter
  matched the expected value (set by `expect_value()`).

* `will_return()` - push a value onto the mock stack _before_ the mocked function
  is called (i.e. called in test code). Expected to be called in a pair
  with `mock()`.

* `mock()` - pop a value from the mock stack. Called inside the mock function.


#### Example Mock

<details>
<summary>Example mocked function and test case.</summary>

```c
#include <dse/testing.h>

/* The real function to be mocked. */
int test_function(int a, int b);

/* The mock function, prefixed with '__wrap_'. */
int __wrap_test_function(int a, int b)
{
    /* Test that the correct parameters were passed to the mock. */
    check_expected(a);
    check_expected(b);
    /* mock() - Pop values from the stack of test values. */
    int a_wrap = mock_type(int);
    int b_wrap = mock_type(int);
    /* Condition/behaviour being mocked. */
    if (a_wrap == 1 && b_wrap == 2) return 3;
    return 0;
}

/* Test functions. */
void test_success(void **state)
{
    /* Push expect values to the mock stack. */
    expect_value(__wrap_test_function, a, 1);
    expect_value(__wrap_test_function, b, 2);
    /* Push values to the mock stack. */
    will_return(__wrap_test_function, 1);
    will_return(__wrap_test_function, 2);
    /* Call the original function, and evaluate the mocked behaviour. */
    assert_int_equal(test_function(1, 2), 3);
}
int main(void)
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(test_success),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

```

</details>


### Inline Python with Cog

> __Info :__ Cog usage is experimental in DSE Test Projects.

Cog is a useful technique for preprocessing Python scripts embedded in C code. Interesting
use cases include:

* Creation of "here documents" (text documents embedded in code files).
* Generation of C code, using `cog.outl()` to write code into the same file during
  cog processing.

#### Example Cog Integration

<details>
<summary>Embedded Cog Heredoc.</summary>

```c
#include <dse/testing.h>

#define UNUSED(x) ((void)x)

/* Cog Heredoc. */
/*[[[cog
yaml="""
kind: SignalGroup
metadata:
  name: network_signals
spec:
  signals:
    - signal: RAW
"""
with open('sg.yaml', 'w') as f:
    f.write(yaml)
]]]*/
/*[[[end]]]*/

int main(void)
{
    /* Test implementation ... */
}
```

</details>
<br />
<details>
<summary>CMake integration.</summary>

```cmake
# Setup preprocessing to generate output files at configuration stage.
execute_process(COMMAND cog.py ${CMAKE_CURRENT_BINARY_DIR}/../../util/test.c)

# GLOB the generated files and install.
file (GLOB GENERATED_YAML_FILES
    ${CMAKE_CURRENT_BINARY_DIR}/../*.yaml
)
install(
    FILES ${GENERATED_YAML_FILES}
    DESTINATION data/yaml
)

# Setup a custom command to invoke cog.
add_custom_command(
    OUTPUT SYMBOLIC test
    COMMAND cog.py  ${CMAKE_CURRENT_BINARY_DIR}/../../util/test.c
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/../../util/test.c
    COMMENT "Creating symbolic file and running cog"
)
```

</details>
<br />
<details>
<summary>Installing Cog.</summary>

```bash
$ pip install cogapp
```

</details>


## References and Further Reading

* [CMocka assert macros](https://api.cmocka.org/group__cmocka__asserts.html)
* [CMocka API](https://api.cmocka.org/group__cmocka.html) - links to checking paramters and mock objects functions.
* [CMocka project page](https://CMocka.org/)
* [Test Driven Development for Embedded C](https://www.oreilly.com/library/view/test-driven-development/9781941222997/) - very good reference on TDD and C.
* [Cog, Inline Python](https://nedbatchelder.com/code/cog/) - An introduction to cog.
