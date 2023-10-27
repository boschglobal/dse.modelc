---
title: "mstep"
linkTitle: "MStep"
draft: false
tags:
- ModelC
- CLI
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc"
---

## mstep

Debug tool to load and step a model through a simulation.


### Synopsis

```bash
# Load and step a Model, specifying the Model, Stack and SignalGroup YAML files.
$ dse.mstep --logger 2 --name binary_model_instance model.yaml stack.yaml signal_group.yaml
```


### Options

```bash
$ mstep --help
Model Loader and Stepper
usage: [--transport <transport>]
       [--uri <endpoint>]  (i.e. redis://localhost:6379)
       [--host <host url>] *** depreciated, use --uri ***
       [--port <port number>] *** depreciated, use --uri ***
       [--stepsize <double>]
       [--endtime <double>]
       [--uid <model uid>]
       [--name <model name>] *** normally required ***
       [--timeout <double>]
       [--logger <number>] 0..6 *** 0=more, 6=less, 3=INFO ***
       [--file <model file>]
       [--path <path to model>] *** relative path to Model Package ***
       [YAML FILE [,YAML FILE] ...]
```
