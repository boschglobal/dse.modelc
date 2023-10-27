---
title: "simbus"
linkTitle: "SimBus"
draft: false
tags:
- ModelC
- CLI
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc"
---

## simbus

Run the SimBus.


### Synopsis

```bash
# Run the SimBus, specifying the Stack YAML file.
$ dse.simbus --logger 2 --timeout 1 stack.yaml
```


### Options

```bash
$ simbus --help
Standalone SimBus
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

