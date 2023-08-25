import pytest

def pytest_addoption(parser):
    parser.addoption("--port",
            help="The port to which the device is attached (eg: /dev/ttyACM0)")
    parser.addoption("--baud", type=int, default=115200,
            help="Serial port baud rate (default: 115200)")
    parser.addoption(
        "--credentials_file", action="store", type=str,
        help="YAML formatted settings file"
    )

@pytest.fixture
def port(request):
    return request.config.getoption("--port")

@pytest.fixture
def baud(request):
    return request.config.getoption("--baud")

@pytest.fixture
def credentials_file(request):
    return request.config.getoption("--credentials_file")
