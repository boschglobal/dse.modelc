---
title: "Model Compatibility Library"
linkTitle: "MCL"
weight: 1020
tags:
  - ModelC
  - Architecture
  - MCL
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc"
---

## Synopsis

The Model Compatibility Library (MCL) of the Dynamic Simulation Environment (DSE)
is a representation of an Architectural Pattern for supporting 3rd-party model
interfaces in a DSE Simulation.
To support a model interface an implementation will need to implement the
MCL API (i.e. MclVTable) and may also optionally use the Marshal API to manage
signal/variable exchange with the model being interfaced.


## Design

## Deployment

## Configuration

## References

* [MCL API]({{< relref "apis/modelc/mcl" >}})
* [Marshal API]({{< relref "apis/clib/marshal" >}})
* [FMI MCL]({{< relref "docs/user/fmi/mcl" >}}) - An MCL implementation for the Modelica FMI Standard.
