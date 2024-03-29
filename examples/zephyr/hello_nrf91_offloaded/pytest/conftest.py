import os
from pathlib import Path
import sys

import west.configuration
import pytest

WEST_TOPDIR = Path(west.configuration.west_dir()).parent
sys.path.insert(0, str(WEST_TOPDIR / 'zephyr' / 'scripts' / 'west_commands'))
from runners.core import BuildConfiguration

def pytest_addoption(parser):
    parser.addoption("--wifi-ssid", type=str, help="WiFi SSID")
    parser.addoption("--wifi-psk",  type=str, help="WiFi PSK")

@pytest.fixture(scope='session')
def anyio_backend():
    return 'trio'

@pytest.fixture(scope='session')
def build_conf(request):
    return BuildConfiguration(request.config.option.build_dir)
