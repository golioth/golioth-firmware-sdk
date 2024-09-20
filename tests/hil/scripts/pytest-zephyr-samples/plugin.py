import pytest
import allure
import os

def pytest_addoption(parser):
    parser.addoption("--wifi-ssid",   type=str, help="WiFi SSID")
    parser.addoption("--wifi-psk",    type=str, help="WiFi PSK")
    parser.addoption("--hil-board",   type=str, help="Simple board name")
    parser.addoption("--runner-name", type=str, help="Self-hosted runner name")

@pytest.fixture(scope="session")
def wifi_ssid(request):
    if request.config.getoption("--wifi-ssid") is not None:
        return request.config.getoption("--wifi-ssid")
    else:
        return os.environ.get('WIFI_SSID')

@pytest.fixture(scope="session")
def wifi_psk(request):
    if request.config.getoption("--wifi-psk") is not None:
        return request.config.getoption("--wifi-psk")
    else:
        return os.environ.get('WIFI_PSK')

@pytest.fixture(scope="session")
def hil_board(request):
    if request.config.getoption("--hil-board") is not None:
        return request.config.getoption("--hil-board")
    else:
        return os.environ['hil_board']

@pytest.fixture(scope="session")
def runner_name(request):
    return request.config.getoption("--runner-name")

@pytest.hookimpl(wrapper=True)
def pytest_runtest_setup(item):
    if item.config.getoption("--hil-board") is not None:
        hil_board = item.config.getoption("--hil-board")
    else:
        hil_board = os.environ['hil_board']

    allure.dynamic.tag(hil_board)
    allure.dynamic.tag("zephyr")
    allure.dynamic.parameter("board_name", hil_board)
    allure.dynamic.parameter("platform_name", "zephyr")
    allure.dynamic.parent_suite(f"sample.zephyr.{hil_board}")

    if runner_name is not None:
        allure.dynamic.tag(item.config.getoption("--runner-name"))

    yield
