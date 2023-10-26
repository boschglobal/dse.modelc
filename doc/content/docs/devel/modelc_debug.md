---
title: "Model C Debug Techniques"
linkTitle: "ModelC Debug"
draft: false
tags:
- Developer
- ModelC
- GDB
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc"
path_base_for_github_subdir: "content/docs/devel/"
---

## GDB

### GDB Enabled Makefiles

Repos can be enabled for interactive GDB debugging with the following technique:

1. In the main Makefile, add the `GDB_CMD` environment variable (i.e. `--env GDB_CMD="$(GDB_CMD)"`) to every `DOCKER_CMD` that requires interactive GDB debugging support.

2. For each Makefile run target where interactive GDB debugging is required, prefix the run command with the `GDB_CMD` variable, for example: `cd build/_out; $(GDB_CMD) bin/test_tdd`.

3. Set the `GDB_CMD` environment variable with your GDB command (e.g. `export GDB_CMD="gdb -ex run"`).

4. Execute your Makefile run target, which will start in GDB according to the configuration in `GDB_CMD`.

A few simple GDB commands:

* **bt** - backtrace.
* **frame x** - move to stack frame x (where x is from output of _bt_ command).
* **print x** - print the value of variable x.
* **list** - list the code around the current stopped point.
* **quit** - exit GDB.

> Hint: this technique is particularly helpful when working with CMocka based Test Cases.


Example GDB_CMD settings:

```bash
# Run GDB.
$ export GDB_CMD="gdb -q -ex run"

# Run GDB and exit if the program finished without error.
$ export GDB_CMD="gdb -q -ex='set confirm on' -ex=run -ex=quit"
```


Example Makefile:

```bash
# Docker CMDS (shortened example).
ifneq ($(CI), true)
	DOCKER_BUILDER_CMD := docker run -it --rm \
		--volume $$(pwd):/tmp/repo \
		...
		--env GDB_CMD="$(GDB_CMD)" \
		--workdir /tmp/repo \
		$(GCC_BUILDER_IMAGE)
endif

# Run target.
run:
	cd build/_out; $(GDB_CMD) bin/test_tdd
```


### ModelC Integration with GDB in Docker Environment

When developing parts of the Model C Library it is occasionally necessary to run
the various elements under GDB. The Docker based build environment can be
configured to do this, and there are several Makefile targets which help in
establishing that environment.

> Exact commands to run SimBus and ModelC can be taken from the output of unit/integration tests.


#### Start Docker Environment (Terminal 1)

This command creates a Docker Network, starts a Redis Container, and then starts
a Docker Build Container ready for use. The current directory of the repo
is also mapped into the container at `/tmp/repo`.


```bash
:~/git/dse/dse.modelc$ make test-env
433d62be46ba4a2af17a03754e240e4e2774a093bceb86bed875dc0c4c9e7ec0
dse
658d227bbdf354525c31d930b9b045c867898498cd37fd3fbbae12e0aec626ca
a035f2d928d18d4df4cfb2215d65e0f75abfb8a9fcdb5f52904d8fff115c4286
:/tmp/repo# ls dse/modelc/build/_out/
bin  data  examples  include  lib  licenses
root@c5e03415542a:/tmp/repo#
```


#### Connect to Docker Environment and start SimBus (Terminal 2)

```bash
:~/git/dse/dse.modelc$ make test-env-it
docker exec -it --workdir /tmp/repo modelc_testenv /bin/bash
:/tmp/repo# cd /tmp/repo/dse/modelc/build/_out/examples/binary
:/tmp/repo/dse/modelc/build/_out/examples/binary# gdb --args /tmp/repo/dse/modelc/build/_out/bin/simbus --logger 2 --timeout 1 stack.yaml
(gdb) run
Starting program: /tmp/repo/dse/modelc/build/_out/bin/simbus --logger 2 --timeout 1 stack.yaml
Version: 0.0.2
...
Start the Bus ...
```


#### Connect to Docker Environment and start ModelC (Terminal 3)

```bash
:~/git/dse/dse.modelc$ make test-env-it
docker exec -it --workdir /tmp/repo modelc_testenv /bin/bash
:/tmp/repo# cd /tmp/repo/dse/modelc/build/_out/examples/binary
:/tmp/repo/dse/modelc/build/_out/examples/binary# gdb --args /tmp/repo/dse/modelc/build/_out/bin/modelc --logger 2 --name binary_model_instance model.yaml stack.yaml signal_group.yaml
(gdb) run
Starting program: /tmp/repo/dse/modelc/build/_out/bin/modelc --logger 2 --name binary_model_instance model.yaml stack.yaml signal_group.yaml
Version: 0.0.2
Platform: linux-amd64
...
Model function model_setup called
Configure Channel: binary_channel
  Channel Name: Network
  Channel Alias: binary_channel
[INFO]     signal[0] : RAW (_load_signals:259)
  Unique signals identified: 1
Init Controller channel: Network
  Pub Key: bus.ch.Network.tx
  Sub Key: bus.ch.Network.rx
[INFO]   Allocate signal vector type 1 for 1 signals. (model_configure_channel:405)
[FATAL]  Binary vector not allocated! (model_setup:94)
[Inferior 1 (process 37) exited with code 0175]
(gdb)
```


#### General GDB commands

* bt
* frame
* up
* down
* info locals
* info args
