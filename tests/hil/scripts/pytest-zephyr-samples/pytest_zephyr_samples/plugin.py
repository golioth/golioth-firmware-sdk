import pytest
import allure
import os
from pathlib import Path
from contextlib import suppress

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
def runner_name(request):
    return request.config.getoption("--runner-name")

@pytest.fixture(scope="session", autouse=True)
def log_files(twister_harness_config):
    yield

    for file in os.listdir(twister_harness_config.devices[0].build_dir):
        if '.log' in file:
            path: Path = twister_harness_config.devices[0].build_dir / file
            allure.attach.file(
                path,
                name=file,
                attachment_type=allure.attachment_type.TEXT,
                extension='log'
            )

@pytest.hookimpl(wrapper=True)
def pytest_runtest_setup(item):
    if item.config.getoption("--hil-board") is not None:
        hil_board = item.config.getoption("--hil-board")
    elif 'hil_board' in os.environ:
        hil_board = os.environ['hil_board']
    else:
        hil_board = item.config.option.device_serial

    with suppress(Exception):
        sample_name = item.config.getoption("--build-dir").split('/')[-1]
        suite_name = sample_name.split("sample.golioth.")[-1]
        allure.dynamic.parameter("sample", sample_name)
        allure.dynamic.suite(suite_name)

    allure.dynamic.tag(hil_board)
    allure.dynamic.tag("zephyr")
    allure.dynamic.parameter("board_name", hil_board)
    allure.dynamic.parameter("platform_name", "zephyr")
    allure.dynamic.parent_suite(f"sample.zephyr.{hil_board}")

    if runner_name is not None:
        allure.dynamic.tag(item.config.getoption("--runner-name"))

    yield
