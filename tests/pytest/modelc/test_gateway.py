#! /usr/bin/python
# Copyright (C) 2021 Robert Bosch GmbH

import os
import asyncio
import pytest


pytestmark = pytest.mark.skipif(
        os.environ["PACKAGE_ARCH"] != "linux-amd64",
        reason="test only runs on linux-amd64")


TIMEOUT = 60
VALGRIND_CMD = 'valgrind --track-origins=yes --leak-check=full --error-exitcode=808'
USE_VALGRIND = True

# Sandbox for Model C
MODELC_SANDBOX_DIR = os.getenv('MODELC_SANDBOX_DIR')
SIMBUS_EXE = MODELC_SANDBOX_DIR+'/bin/simbus'
MODELC_EXE = MODELC_SANDBOX_DIR+'/bin/modelc'

# Sandbox for the Gateway Model (test basis).
GATEWAY_SANDBOX_DIR = os.getenv('MODELC_SANDBOX_DIR')+'/examples/gateway'
GATEWAY_YAML = 'data/gateway.yaml'
GATEWAY_EXE = 'bin/gateway'
GATEWAY_INST = 'gateway'
STACK_YAML = 'data/gateway.yaml'


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
            f'{SIMBUS_EXE} --logger 2 --timeout 1 {STACK_YAML}'),
        timeout=TIMEOUT)
    )
    # Models
    for m in params['models']:
        run_list.append(asyncio.wait_for(run(
            params['MODEL_SANDBOX_DIR'],
            f'{MODELC_EXE} --logger 2 ' + f'--name {m["MODEL_INST"]} ' + f'{m["MODEL_YAML_FILES"]} ' + ''), timeout=TIMEOUT))
    # Gateway
    run_list.append(asyncio.wait_for(run(
            f'{GATEWAY_SANDBOX_DIR}',
            f'{GATEWAY_EXE} 0.005 0.02 {GATEWAY_YAML}'),
        timeout=TIMEOUT)
    )

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


def test_gateway():
    params = {
        'MODEL_SANDBOX_DIR': os.getenv('MODELC_SANDBOX_DIR')+'/examples/gateway',
        'models': [],
    }
    checks = [
        '[0.000000] binary[0] = <0:0>(null) (binary_foo)',
        '[0.000000] binary[1] = <0:0>(null) (binary_bar)',
        '[0.020000] binary[0] = <20:20>st=0.015000,index=0 (binary_foo)',
        '[0.020000] binary[1] = <20:20>st=0.015000,index=1 (binary_bar)',
        '[0.000000] scalar[0] = 0.000000 (scalar_foo)',
        '[0.000000] scalar[1] = 0.000000 (scalar_bar)',
        '[0.020000] scalar[0] = 16.000000 (scalar_foo)',
        '[0.020000] scalar[1] = 32.000000 (scalar_bar)',
    ]
    asyncio.run(main(params, checks))
