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

# Sandbox for Model C API
MODELC_SANDBOX_DIR = os.getenv('MODELC_SANDBOX_DIR')
MSTEP_EXE = MODELC_SANDBOX_DIR+'/bin/mstep'

# Sandbox for the Minimal Model (test basis).
MODEL_SANDBOX_DIR = os.getenv('MODELC_SANDBOX_DIR')+'/examples/minimal'
DYNAMIC_MODEL_INST = 'minimal_inst'
DYNAMIC_MODEL_YAML = 'data/model.yaml'
DYNAMIC_MODEL_LIB = 'lib/libminimal.so'  # Not used, see model.yaml.
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


async def main():
    result_list = await asyncio.gather(
        asyncio.wait_for(
            run(
                MODEL_SANDBOX_DIR, f'{MSTEP_EXE} --logger 2 --name {DYNAMIC_MODEL_INST} ' + f'{DYNAMIC_MODEL_YAML} ' + f'{STACK_YAML} ' + ''), timeout=TIMEOUT),
                )
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

    for result in result_list:
        assert result['rc'] == 0
        assert 'value: 10' in result['stdout']
        assert 'Starting Simulation (for 10 steps) ...' in result['stdout']
        assert 'Model Function: model_step' in result['stdout']


if __name__ == '__main__':
    asyncio.run(main())


def test_mstep():
    asyncio.run(main())
