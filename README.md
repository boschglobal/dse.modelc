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

### User Guide

* [Simer - Simulation Runner](https://boschglobal.github.io/dse.doc/docs/user/simer/)

### Project Structure

```text
dse.modelc
├── doc                     <-- Project documentation.
├── dse/modelc              <-- Model C Library source code.
├── extra
│   ├── go                  <-- Go packages.
│   └── tools
│       ├── simer           <-- Simer simulation runner.
│       └── trace           <-- Simulation trace tool.
├── licenses                <-- Third-party licenses.
├── tests
│   ├── cmocka              <-- Unit and integration tests (C/CMocka).
│   └── e2e                 <-- End-to-end (E2E) tests.
├── Makefile                <-- Repo-level Makefile.
└── Taskfile.yml            <-- Taskfile containing supporting automation.
```

## Usage

Simulations can be run using the Simer tool, which is part of this project.

```bash
# Define a shell function for the Simer tool.
export SIMER_IMAGE=ghcr.io/boschglobal/dse-simer:latest
simer() { ( cd "$1" && shift && docker run -it --rm -v "$(pwd):/sim" "$SIMER_IMAGE" "$@"; ) }

# Download and run a simulation example.
export MODELC_URL=https://github.com/boschglobal/dse.modelc/releases/download/v2.0.7/ModelC-2.0.7-linux-amd64.zip
curl -fSL -o /tmp/modelc.zip "$MODELC_URL"; unzip -d /tmp /tmp/modelc.zip
simer /tmp/ModelC-2.0.7-linux-amd64/examples/minimal

# Or build locally and run an example simulation.
make
simer dse/modelc/build/_out/examples/minimal
```

> Hint: Replace `v2.0.7` with the latest version from the [releases page](https://github.com/boschglobal/dse.modelc/releases).

> Hint: Find more information about the Simer [command options here](https://boschglobal.github.io/dse.doc/docs/user/simer/#options).

## Build

> Note: see the following section on configuring toolchains.

```bash
# Clone the repo.
git clone https://github.com/boschglobal/dse.modelc.git
cd dse.modelc

# Optionally set builder images.
export GCC_BUILDER_IMAGE=ghcr.io/boschglobal/dse-gcc-builder:latest

# Build.
make

# Build containerised tools.
make tools

# Run tests (using local images).
export SIMER_IMAGE=simer:test
export SIMER_IMAGE=simer:test-slim  # Alternative slim image.
make test

# Build checks for all architectures.
make arch

# Remove temporary build artifacts.
make clean
make cleanall
```

### Toolchains

The Model C Library is built using containerised toolchains. The required toolchains are provided by the DSE C Library and can be built as follows:

```bash
git clone https://github.com/boschglobal/dse.clib.git
cd dse.clib
make docker
```

Alternatively, the latest Docker images are available on ghcr.io and can be used as follows:

```bash
export GCC_BUILDER_IMAGE=ghcr.io/boschglobal/dse-gcc-builder:latest
```

## Contribute

Please refer to the [CONTRIBUTING.md](./CONTRIBUTING.md) file.

## License

This project is open-sourced under the Apache-2.0 license.
See the [LICENSE](LICENSE) and [NOTICE](./NOTICE) files for details.

### Third-Party Licenses

[Third-Party Licenses](licenses/)