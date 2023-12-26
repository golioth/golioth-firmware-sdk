import os
import pytest

def pytest_addoption(parser):
    parser.addoption("--credentials-file", type=str,
                     help="YAML formatted settings file")

@pytest.fixture(scope='session')
def anyio_backend():
    return 'trio'

@pytest.fixture(scope="session")
def credentials_file(request):
    if request.config.getoption("--credentials-file") is not None:
        return request.config.getoption("---credentials-file")
    else:
        return os.environ['GOLIOTH_CREDENTIALS_FILE']
