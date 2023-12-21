<!--
Copyright 2023 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# Dynamic Simulation Environment - Model C Library

[![CI](https://github.com/boschglobal/dse.modelc/actions/workflows/ci.yaml/badge.svg)](https://github.com/boschglobal/dse.modelc/actions/workflows/ci.yaml)
[![Super Linter](https://github.com/boschglobal/dse.modelc/actions/workflows/super_linter.yaml/badge.svg)](https://github.com/boschglobal/dse.modelc/actions/workflows/super_linter.yaml)
![GitHub](https://img.shields.io/github/license/boschglobal/dse.modelc)


## Introduction

Model C Library of the Dynamic Simulation Environment (DSE) Core Platform.


### Project Structure

```
L- dse/modelc   Model C Library source code.
L- extra        Build infrastructure.
  L- docker     Containerised tools.
L- licenses     Third Party Licenses.
L- tests        Unit and integration tests.
```


## Usage

### Toolchains

The Model C Library is built using containerised toolchains. Those are
available from the DSE C Library and can be built as follows:

```bash
$ git clone https://github.com/boschglobal/dse.clib.git
$ cd dse.clib
$ make docker
```

Alternatively, the latest Docker Images are available on ghcr.io and can be
used as follows:

```bash
$ export GCC_BUILDER_IMAGE=ghcr.io/boschglobal/dse-gcc-builder:main
$ export GCC_TESTER_IMAGE=ghcr.io/boschglobal/dse-python-builder:main
```


### Build

```bash
# Get the repo.
$ git clone https://github.com/boschglobal/dse.modelc.git
$ cd dse.modelc

# Optionally set builder images.
$ export GCC_BUILDER_IMAGE=ghcr.io/boschglobal/dse-gcc-builder:main
$ export GCC_TESTER_IMAGE=ghcr.io/boschglobal/dse-python-builder:main

# Build.
$ make

# Run tests.
$ make test

# Build containerised tools.
$ make docker

# Remove (clean) temporary build artifacts.
$ make clean
$ make cleanall
```


## Contribute

Please refer to the [CONTRIBUTING.md](./CONTRIBUTING.md) file.


## License

Dynamic Simulation Environment Model C Library is open-sourced under the
Apache-2.0 license.
See the [LICENSE](LICENSE) and [NOTICE](./NOTICE) files for details.


### Third Party Licenses

[Third Party Licenses](licenses/)
