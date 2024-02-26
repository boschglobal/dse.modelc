#! /usr/bin/python
# Copyright (C) 2024 Robert Bosch GmbH

import os
import asyncio
import pytest


pytestmark = pytest.mark.skipif(
        os.environ["PACKAGE_ARCH"] != "linux-amd64",
        reason="test only runs on linux-amd64")


TIMEOUT = 60
VALGRIND_CMD = 'valgrind --track-origins=yes --leak-check=full --num-callers=20 --error-exitcode=808'
USE_VALGRIND = True

# Sandbox for Model C
MODELC_SANDBOX_DIR = os.getenv('MODELC_SANDBOX_DIR')
SIMBUS_EXE = MODELC_SANDBOX_DIR+'/bin/simbus'
MODELC_EXE = MODELC_SANDBOX_DIR+'/bin/modelc'

# Sandbox for the Model (common only, specific in test cases).
MODEL_YAML = 'data/model.yaml'
STACK_YAML = 'data/simulation.yaml'


async def run(dir, cmd):
    _cmd = f'cd {dir}; {VALGRIND_CMD if USE_VALGRIND else ""} {cmd}'
    print('Run:')
    print(f'  dir: {dir}')
    print(f'  cmd: {_cmd}')
    p = await asyncio.create_subprocess_shell(
        _cmd,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )
    stdout, stderr = await p.communicate()
    return { 'dir': dir, 'cmd': _cmd, 'rc': p.returncode, 'stdout': stdout.decode(), 'stderr': stderr.decode() }


async def main(params, checks):
    run_list = []
    # SimBus
    run_list.append(asyncio.wait_for(run(
            params['MODEL_SANDBOX_DIR'],
            f'{SIMBUS_EXE} --logger 2 --timeout 1 --transport {params["transport"]} --uri {params["uri"]} {STACK_YAML}'),
        timeout=TIMEOUT)
    )
    # Models
    for m in params['models']:
        run_list.append(asyncio.wait_for(
            run(
                params['MODEL_SANDBOX_DIR'],
                f'{MODELC_EXE} --logger 2 ' +
                f'--transport {params["transport"]} ' +
                f'--uri {params["uri"]} ' +
                f'--name {m["MODEL_INST"]} ' +
                f'{m["MODEL_YAML_FILES"]} ' + ''
            ), timeout=TIMEOUT))
    # Gather results.
    result_list = await asyncio.gather(*run_list)
    for result in result_list:
        print('************************************************************')
        print('************************************************************')
        print('************************************************************')
        print(f'CMD : {result["cmd"]}')
        print('++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++')
        print(f'STDOUT : \n{result["stdout"]}')
        print('++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++')
        print(f'STDERR : \n{result["stderr"]}')
        print('++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++')
        print(f'RC : {result["rc"]}')
        print('************************************************************')
        print('')
        assert result['rc'] == 0

    for check in checks:
        # Search for check in each result STDOUT
        found = False
        for result in result_list:
            if check in result['stdout']:
                found = True
        assert found, check


@pytest.mark.parametrize("transport,uri", [
    ("redispubsub", "redis://redis:6379"),
    ("redis", "redis://redis:6379"),
    ("redis", "redisasync://redis:6379"),

    # Transports not currently supported in test environment
    # ("redispubsub", "unix:///tmp/redis/redis.sock"),
    # ("mq", "posix:///sim"),
])
def test_modelc_transport(transport, uri):
    params = {
        'transport' : transport,
        'uri' : uri,
        'MODEL_SANDBOX_DIR': os.getenv('MODELC_SANDBOX_DIR')+'/examples/minimal',
        'models': [
            {
                'MODEL_INST': 'minimal_inst',
                'MODEL_YAML_FILES': f'{MODEL_YAML} ' + f'{STACK_YAML} ',
            },
        ]
    }
    checks = [
        'SignalValue: 2628574755 = 4.000000 [name=counter]',
    ]
    asyncio.run(main(params, checks))
