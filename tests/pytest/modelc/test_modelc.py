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
            f'{SIMBUS_EXE} --logger 2 --timeout 1 {STACK_YAML}'),
        timeout=TIMEOUT)
    )
    # Models
    for m in params['models']:
        run_list.append(asyncio.wait_for(run(
            params['MODEL_SANDBOX_DIR'],
            f'{MODELC_EXE} --logger 2 ' + f'--name {m["MODEL_INST"]} ' + f'{m["MODEL_YAML_FILES"]} ' + ''), timeout=TIMEOUT))
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


def test_modelc_minimal():
    params = {
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


def test_modelc_extended():
    params = {
        'MODEL_SANDBOX_DIR': os.getenv('MODELC_SANDBOX_DIR')+'/examples/extended',
        'models': [
            {
                'MODEL_INST': 'extended_inst',
                'MODEL_YAML_FILES': f'{MODEL_YAML} ' + f'{STACK_YAML} ',
            },
        ]
    }
    checks = [
        'SignalWrite: 2628574755 = 43.000000 [name=counter]',
        'SignalWrite: 2906692958 = 1.000000 [name=odd]',
        'SignalWrite: 3940322353 = 0.000000 [name=even]',
        'SignalValue: 2628574755 = 46.000000 [name=counter]',
        'SignalValue: 2906692958 = 0.000000 [name=odd]',
        'SignalValue: 3940322353 = 1.000000 [name=even]',
    ]
    asyncio.run(main(params, checks))


#@pytest.mark.skipif(os.environ["PACKAGE_ARCH"] != "linux-amd64", reason="test only runs on linux-amd64")
def test_modelc_binary():
    params = {
        'MODEL_SANDBOX_DIR': os.getenv('MODELC_SANDBOX_DIR')+'/examples/binary',
        'models': [
            {
                'MODEL_INST': 'binary_inst',
                'MODEL_YAML_FILES' : f'{MODEL_YAML} ' + f'{STACK_YAML} ',
            },
        ]
    }
    checks = [
        'Message (message) : count is 43',
        'Message (message) : count is 44',
        'Message (message) : count is 45',
        'Message (message) : count is 46',
    ]
    asyncio.run(main(params, checks))


def test_modelc_ncodec():
    params = {
        'MODEL_SANDBOX_DIR': os.getenv('MODELC_SANDBOX_DIR')+'/examples/ncodec',
        'models': [
            {
                'MODEL_INST': 'ncodec_inst',
                'MODEL_YAML_FILES' : f'{MODEL_YAML} ' + f'{STACK_YAML} ',
            },
        ]
    }
    checks = [
        'RX (03e9): Hello World! from node_id=24',
        'RX (03ea): Hello World! from node_id=24',
        'RX (03eb): Hello World! from node_id=24',
        'RX (03ec): Hello World! from node_id=24',
    ]
    asyncio.run(main(params, checks))
