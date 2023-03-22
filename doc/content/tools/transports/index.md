---
title: "Transports"
draft: true
---

# Transports

## Redis PubSub

### Usage TCP

#### CLI

```bash
$ simbus stack.yaml --transport redispubsub --uri redis://localhost:6379
$ modelc --name instance model.yaml --transport redispubsub --uri redis://localhost:6379
```


#### Stack (YAML)

```yaml
---
kind: Stack
metadata:
  name: stack
spec:
  connection:
    transport:
      redispubsub:
        uri: redis://localhost:6379
```


### Usage UNIX Socket File

#### CLI

```bash
$ simbus stack.yaml --transport redispubsub --uri unix:///tmp/redis/redis.sock
$ modelc --name instance model.yaml --transport redispubsub --uri unix:///tmp/redis/redis.sock
```


#### Stack (YAML)

```yaml
---
kind: Stack
metadata:
  name: stack
spec:
  connection:
    transport:
      redispubsub:
        uri: unix:///tmp/redis/redis.sock
```



## Redis

> Note: Not currently available.



## Message Queue - POSIX

The POSIX based Message Queue can be selected as a transport with the `mq`
transport selector and a `posix` URI. This can be specified at the CLI or in
a Stack YAML file (examples follow).

> Note: Linux only.


### Host System Settings

Configure the Message Queue subsystem with the following commands/configuration.

```bash
# Edit the sysctl configuration, adding the following items.
$ sudo nano /etc/sysctl.conf
fs.mqueue.msg_max = 50
fs.mqueue.msgsize_max = 65536
fs.mqueue.queues_max = 256

# Edit the limits configuration, adjust/append the following items.
$ sudo nano /etc/security/limits.conf
* soft msgqueue -1
* hard msgqueue -1

# Restart the system, then check the parameters.
$ sysctl fs.mqueue
fs.mqueue.msg_default = 10
fs.mqueue.msg_max = 50
fs.mqueue.msgsize_default = 8192
fs.mqueue.msgsize_max = 65536
fs.mqueue.queues_max = 256

$ ulimit -a
core file size          (blocks, -c) 0
data seg size           (kbytes, -d) unlimited
scheduling priority             (-e) 0
file size               (blocks, -f) unlimited
pending signals                 (-i) 15675
max locked memory       (kbytes, -l) 65536
max memory size         (kbytes, -m) unlimited
open files                      (-n) 1024
pipe size            (512 bytes, -p) 8
POSIX message queues     (bytes, -q) unlimited
real-time priority              (-r) 0
stack size              (kbytes, -s) 8192
cpu time               (seconds, -t) unlimited
max user processes              (-u) 15675
virtual memory          (kbytes, -v) unlimited
file locks                      (-x) unlimited
```


### Usage

#### CLI

```bash
$ simbus stack.yaml --transport mq --uri posix:///stem
$ modelc --name instance model.yaml --transport mq --uri posix:///stem
```


#### Stack (YAML)

```yaml
---
kind: Stack
metadata:
  name: stack
spec:
  connection:
    transport:
      mq:
        uri: posix:///stem
```


## Message Queue - Named Pipe


> Note: Windows only (currently).


### Usage

#### CLI

```bash
$ simbus stack.yaml --transport mq --uri namedpipe:///stem
$ modelc --name instance model.yaml --transport mq --uri namedpipe:///stem
```


#### Stack (YAML)

```yaml
---
kind: Stack
metadata:
  name: stack
spec:
  connection:
    transport:
      mq:
        uri: namedpipe:///stem
```