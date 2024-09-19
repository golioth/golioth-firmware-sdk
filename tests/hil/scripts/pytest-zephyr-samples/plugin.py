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

@pytest.fixture(autouse=True, scope="session")
def add_allure_report_parent_suite(hil_board):
    # Set the full Allure suite name in case there is a failure during a fixture (before a test
    # function runs).
    allure.dynamic.parent_suite(f"sample.zephyr.{hil_board}")

@pytest.fixture(autouse=True, scope="function")
def add_allure_report_device_and_platform(hil_board, runner_name):
    # Set the Allure information for every test function.
    # Especially important are the parameters which identify variants of the same test
    allure.dynamic.tag(hil_board)
    allure.dynamic.tag("zephyr")
    allure.dynamic.parameter("board_name", hil_board)
    allure.dynamic.parameter("platform_name", "zephyr")
    allure.dynamic.parent_suite(f"sample.zephyr.{hil_board}")

    if runner_name is not None:
        allure.dynamic.tag(runner_name)
