---
title: "modelc"
linkTitle: "ModelC"
draft: false
tags:
- ModelC
- CLI
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc"
---

## modelc

Run the model loader and executer.


### Synopsis

```bash
# Load and run a Model, specifying the Model, Stack and SignalGroup YAML files.
$ dse.modelc --logger 2 --name binary_model_instance model.yaml stack.yaml signal_group.yaml
```


### Options

```bash
$ dse.modelc
ModelC - model runner
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

