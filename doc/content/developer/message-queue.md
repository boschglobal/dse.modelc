---
title: "Message Queue Developer Guide"
linkTitle: "Message Queues"
draft: false
tags:
- Developer
- ModelC
- MQ
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc/content/developer"
path_base_for_github_subdir: "content/docs/developer/"
---

## Message Queue Developer Guide

### Transport: Posix MQ

#### OS Setup

 * $ sudo sh -c "echo 50 > /proc/sys/fs/mqueue/msg_max"
 * $ sudo sh -c "echo 16384 > /proc/sys/fs/mqueue/msgsize_max"
 * $ sudo sh -c "echo 256 > /proc/sys/fs/mqueue/queues_max"
 *
 * $ sysctl fs.mqueue
 * $ sysctl fs.mqueue.queues_max = 256
 *
 *
 * Needs to be MSGSIZE * MAXMSG
 * $ ulimit -a
 * $ ulimit -q
 * /etc/security/limits.conf (set to -1 for no limits)
 *    * soft msgqueue -1
 *    * hard msgqueue -1


#### Commands

cd /vagrant/git/dse.modelcapi/examples/dynamic_model/build/_out

/vagrant/git/dse.modelcapi/libmodelcapi/build/_out/bin/simbus --logger 1 data/yaml/dynamic_model/dynamic_model.yaml --transport mq --uri posix

/vagrant/git/dse.modelcapi/libmodelcapi/build/_out/bin/modelc --logger 1 --name dynamic_model_instance data/yaml/dynamic_model/dynamic_model.yaml --transport mq --uri posix



#### Benchmarking

Adjust the valgrind.py with parameters:

    --endtime 100
    --logger 3
    --transport mq
    --uri posix

increase the timeout too.


redis

32:42 - 33:00 = 18
33:26 - 33:45 = 19

mq posix

36:39 - 36:43 = 4
35:21 - 35:24 = 3

