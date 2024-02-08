import os
import pytest

def pytest_addoption(parser):
    parser.addoption("--wifi-ssid", type=str, help="WiFi SSID")
    parser.addoption("--wifi-psk",  type=str, help="WiFi PSK")

@pytest.fixture(scope='session')
def anyio_backend():
    return 'trio'

@pytest.fixture(scope="session")
def wifi_ssid(request):
    if request.config.getoption("--wifi-ssid") is not None:
        return request.config.getoption("--wifi-ssid")
    else:
        return os.environ['WIFI_SSID']

@pytest.fixture(scope="session")
def wifi_psk(request):
    if request.config.getoption("--wifi-psk") is not None:
        return request.config.getoption("--wifi-psk")
    else:
        return os.environ['WIFI_PSK']
