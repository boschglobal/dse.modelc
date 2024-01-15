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



## Message Queue POSIX

The POSIX based Message Queue can be selected as a transport with the `mq`
transport selector and a `posix` URI. This can be specified at the CLI or in
a Stack YAML file (examples follow).

> Note: Linux only.

> Note: Model stacks not supported. Single model instances of ModelC only.

> Note: Model UID should be configured in either stack.yaml or the ModelC CLI.


### Host System Settings

Configure the Message Queue subsystem with the following commands/configuration.

```bash
# Edit the sysctl configuration, adding the following items.
$ sudo nano /etc/sysctl.conf
fs.mqueue.msg_default = 100
fs.mqueue.msg_max = 100
fs.mqueue.msgsize_max = 1048576
fs.mqueue.msgsize_default = 1048576
fs.mqueue.queues_max = 256

# Edit the limits configuration, adjust/append the following items.
$ sudo nano /etc/security/limits.conf
* soft msgqueue -1
* hard msgqueue -1

# Restart the system, then check the parameters.
$ sysctl fs.mqueue
fs.mqueue.msg_default = 100
fs.mqueue.msg_max = 100
fs.mqueue.msgsize_default = 1048576
fs.mqueue.msgsize_max = 1048576
fs.mqueue.queues_max = 256

$ ulimit -q
POSIX message queues     (bytes, -q) unlimited

# Alternative, useful for debugging.
$ ulimit -Hq 100000
$ ulimit -Sq 100000
```


#### More Info

* https://stackoverflow.com/questions/60275031/how-to-set-posix-message-queues-limit-to-unlimited-in-docker-container-using-u
* https://man7.org/linux/man-pages/man7/mq_overview.7.html


### Usage

The `stem` part of the URI should be unique to the running simulation.


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
