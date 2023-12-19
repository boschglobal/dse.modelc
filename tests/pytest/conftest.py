import logging
import pytest


logging.basicConfig(
    # level='DEBUG',
    level='INFO',
    # level='WARNING',
    format='%(asctime)s %(name)s: %(levelname)s: %(message)s (%(module)s:%(funcName)s.%(lineno)d)',
)


def pytest_runtest_makereport(item, call):
    if "incremental" in item.keywords:
        if call.excinfo is not None:
            parent = item.parent
            parent._previousfailed = item


def pytest_runtest_setup(item):
    if "incremental" in item.keywords:
        previousfailed = getattr(item.parent, "_previousfailed", None)
        if previousfailed is not None:
            pytest.xfail("previous test failed (%s)" % previousfailed.name)
