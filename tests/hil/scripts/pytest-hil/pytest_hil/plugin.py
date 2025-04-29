import pytest
import allure
import os
from pytest_hil.espidfboard import ESPIDFBoard
from pytest_hil.esp32_devkitc_wrover import ESP32DevKitCWROVER
from pytest_hil.frdmrw612 import FRDMRW612
from pytest_hil.nrf52840dk import nRF52840DK
from pytest_hil.nrf9160dk  import nRF9160DK
from pytest_hil.rak5010 import RAK5010
from pytest_hil.linuxboard import LinuxBoard
from pytest_hil.native_sim import NativeSimBoard

def pytest_addoption(parser):
    parser.addoption("--allure-platform", type=str,
            help="Allure platform name (eg: esp-idf, linux, zephyr)")
    parser.addoption("--runner-name", type=str,
            help="Self-hosted runner name (eg: sams_orange_pi, mikes_testbench)")
    parser.addoption("--board", type=str,
            help="The board being tested")
    parser.addoption("--port",
            help="The port to which the device is attached (eg: /dev/ttyACM0)")
    parser.addoption("--baud", type=int, default=115200,
            help="Serial port baud rate (default: 115200)")
    parser.addoption("--fw-image", type=str,
            help="Firmware binary to program to device")
    parser.addoption("--serial-number", type=str,
            help="Serial number to identify on-board debugger")
    parser.addoption("--bmp-port", type=str,
            help="Serial port of a BlackMagicProbe programmer")
    parser.addoption("--custom-suitename", type=str,
            help="Use a custom suite name when generating Allure reports")
    parser.addoption("--allure-board", type=str,
            help="Use a custom board name for Allure reports")
    parser.addoption("--wifi-ssid", type=str, help="WiFi SSID")
    parser.addoption("--wifi-psk", type=str, help="WiFi PSK")


@pytest.fixture(scope="session")
def allure_platform_name(request):
    return request.config.getoption("--allure-platform")

@pytest.fixture(scope="session")
def runner_name(request):
    return request.config.getoption("--runner-name")

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
def fw_image(request):
    return request.config.getoption("--fw-image")

@pytest.fixture(scope="session")
def serial_number(request):
    return request.config.getoption("--serial-number")

@pytest.fixture(scope="session")
def bmp_port(request):
    if request.config.getoption("--bmp-port") is not None:
        return request.config.getoption("--bmp-port")
    else:
        return os.environ.get('CI_BLACKMAGICPROBE_PORT')

@pytest.fixture(scope="session")
def custom_suitename(request):
    return request.config.getoption("--custom-suitename")

@pytest.fixture(scope="session")
def wifi_ssid(request):
    return request.config.getoption("--wifi-ssid")

@pytest.fixture(scope="session")
def wifi_psk(request):
    return request.config.getoption("--wifi-psk")

@pytest.fixture(scope="module")
async def board(request, baud, wifi_ssid, wifi_psk):
    if request.config.getoption('twister_harness', None):
        dut = request.getfixturevalue("dut")
        dut.disconnect()

        board_name = dut.device_config.platform \
            .split("/", maxsplit=1)[0] \
            .split("@", maxsplit=1)[0]
        port = dut.device_config.serial
        fw_image = None
        serial_number = None
        bmp_port = None
    else:
        board_name = request.getfixturevalue("board_name")
        port = request.getfixturevalue("port")
        fw_image = request.getfixturevalue("fw_image")
        serial_number = request.getfixturevalue("serial_number")
        bmp_port = request.getfixturevalue("bmp_port")

    if (board_name.lower() == "esp32s3_devkitc_espidf" or
        board_name.lower() == "esp32c3_devkitm_espidf" or
        board_name.lower() == "esp32_devkitc_wrover_espidf"):
        board = ESPIDFBoard(port, baud, wifi_ssid, wifi_psk, fw_image, serial_number=serial_number)
    elif board_name.lower() == "esp32_devkitc_wrover":
        board = ESP32DevKitCWROVER(port, baud, wifi_ssid, wifi_psk, fw_image,
                                   serial_number=serial_number)
    elif board_name.lower() == "frdm_rw612":
        board = FRDMRW612(port, baud, wifi_ssid, wifi_psk, fw_image, serial_number=serial_number)
    elif board_name.lower() == "nrf52840dk":
        board = nRF52840DK(port, baud, wifi_ssid, wifi_psk, fw_image, serial_number=serial_number)
    elif board_name.lower() == "nrf9160dk":
        board = nRF9160DK(port, baud, wifi_ssid, wifi_psk, fw_image, serial_number=serial_number)
    elif board_name.lower() == "native_sim":
        board = NativeSimBoard(fw_image)
    elif board_name.lower() == "rak5010":
        board = RAK5010(port, baud, wifi_ssid, wifi_psk, fw_image, bmp_port=bmp_port)
    elif board_name.lower() == "linux":
        board = LinuxBoard(fw_image)
    else:
        raise ValueError("Unknown board")

    async with board.started():
        yield board

@pytest.hookimpl(wrapper=True)
def pytest_runtest_setup(item):
    board_name = item.config.getoption("--allure-board") or item.config.getoption("--board")
    allure_platform_name = item.config.getoption("--allure-platform")
    suitename = item.config.getoption("--custom-suitename") or "hil"

    allure.dynamic.tag(board_name)
    allure.dynamic.tag(allure_platform_name)
    allure.dynamic.parameter("board_name", board_name)
    allure.dynamic.parameter("platform_name", allure_platform_name)
    allure.dynamic.parent_suite(f"{suitename}.{allure_platform_name}.{board_name}")

    if runner_name is not None:
        allure.dynamic.tag(item.config.getoption("--runner-name"))

    yield
