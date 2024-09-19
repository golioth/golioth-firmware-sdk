import pytest
import os

@pytest.fixture(scope='session')
def anyio_backend():
    return 'trio'

def pytest_configure(config):
    suite_name = os.path.basename(os.path.dirname(os.path.realpath(__file__)))
    config.inicfg["junit_suite_name"] = suite_name
