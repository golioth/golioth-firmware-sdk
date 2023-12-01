import os
import pytest

def pytest_addoption(parser):
    parser.addoption("--api-key", type=str,
                     help="Golioth API key")
    parser.addoption("--device-name", type=str,
                     help="Golioth device name")
    parser.addoption("--credentials-file", type=str,
                     help="YAML formatted settings file")


@pytest.fixture(scope='session')
def anyio_backend():
    return 'trio'

@pytest.fixture(scope="session")
def api_key(request):
    if request.config.getoption("--api-key") is not None:
        return request.config.getoption("--api-key")
    else:
        return os.environ['GOLIOTH_API_KEY']

@pytest.fixture(scope="session")
def credentials_file(request):
    if request.config.getoption("--credentials-file") is not None:
        return request.config.getoption("---credentials-file")
    else:
        return os.environ['GOLIOTH_CREDENTIALS_FILE']

@pytest.fixture(scope="session")
def device_name(request):
    if request.config.getoption("--device-name") is not None:
        return request.config.getoption("--device-name")
    else:
        return os.environ['GOLIOTH_DEVICE_NAME']
