import pytest
from espidfboard import ESPIDFBoard
from esp32_devkitc_wrover import ESP32DevKitCWROVER
from nrf52840dk import nRF52840DK
from nrf9160dk  import nRF9160DK
from mimxrt1024evk import MIMXRT1024EVK
from linuxboard import LinuxBoard
from native_sim import NativeSimBoard

def pytest_addoption(parser):
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
    parser.addoption("--wifi-ssid", type=str, help="WiFi SSID")
    parser.addoption("--wifi-psk", type=str, help="WiFi PSK")


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
def wifi_ssid(request):
    return request.config.getoption("--wifi-ssid")

@pytest.fixture(scope="session")
def wifi_psk(request):
    return request.config.getoption("--wifi-psk")

@pytest.fixture(scope="module")
async def board(board_name, port, baud, wifi_ssid, wifi_psk, fw_image, serial_number):
    if (board_name.lower() == "esp32s3_devkitc_espidf" or
        board_name.lower() == "esp32c3_devkitm_espidf" or
        board_name.lower() == "esp32_devkitc_wrover_espidf"):
        board = ESPIDFBoard(port, baud, wifi_ssid, wifi_psk, fw_image, serial_number)
    elif board_name.lower() == "esp32_devkitc_wrover":
        board = ESP32DevKitCWROVER(port, baud, wifi_ssid, wifi_psk, fw_image, serial_number)
    elif board_name.lower() == "nrf52840dk":
        board = nRF52840DK(port, baud, wifi_ssid, wifi_psk, fw_image, serial_number)
    elif board_name.lower() == "nrf9160dk":
        board = nRF9160DK(port, baud, wifi_ssid, wifi_psk, fw_image, serial_number)
    elif board_name.lower() == "mimxrt1024_evk":
        board = MIMXRT1024EVK(port, baud, wifi_ssid, wifi_psk, fw_image, serial_number)
    elif board_name.lower() == "native_sim":
        board = NativeSimBoard(fw_image)
    elif board_name.lower() == "linux":
        board = LinuxBoard(fw_image)
    else:
        raise ValueError("Unknown board")

    async with board.started():
        yield board
