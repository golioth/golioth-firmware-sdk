import os
import pytest

def pytest_addoption(parser):
    parser.addoption("--credentials-file", type=str,
                     help="YAML formatted settings file")
    parser.addoption("--west-board", type=str,
                     help="Name of the board as specified in the Zephyr tree")

@pytest.fixture(scope='session')
def anyio_backend():
    return 'trio'

@pytest.fixture(scope="session")
def credentials_file(request):
    if request.config.getoption("--credentials-file") is not None:
        return request.config.getoption("---credentials-file")
    else:
        return os.environ['GOLIOTH_CREDENTIALS_FILE']

@pytest.fixture(scope="session")
def west_board(request):
    if request.config.getoption("--west-board") is not None:
        return request.config.getoption("--west-board")
    else:
        return os.environ['west_board']
