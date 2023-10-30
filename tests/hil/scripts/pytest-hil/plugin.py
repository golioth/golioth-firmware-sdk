import pytest
from nrf52840dk import nRF52840DK

def pytest_addoption(parser):
    parser.addoption("--board", type=str,
            help="The board being tested")
    parser.addoption("--port",
            help="The port to which the device is attached (eg: /dev/ttyACM0)")
    parser.addoption("--baud", type=int, default=115200,
            help="Serial port baud rate (default: 115200)")
    parser.addoption(
        "--credentials-file", action="store", type=str,
        help="YAML formatted settings file"
    )

@pytest.fixture(scope="session")
def board_name(request):
    return request.config.getoption("--board")

@pytest.fixture(scope="session")
def port(request):
    return request.config.getoption("--port")

@pytest.fixture(scope="session")
def baud(request):
    return request.config.getoption("--baud")

@pytest.fixture(scope="session")
def credentials_file(request):
    return request.config.getoption("--credentials-file")

@pytest.fixture(scope="module")
def board(board_name, port, baud, credentials_file):
    if board_name.lower() == "nrf52840dk":
        return nRF52840DK(port, baud, credentials_file)
    else:
        raise ValueError("Unknown board")
