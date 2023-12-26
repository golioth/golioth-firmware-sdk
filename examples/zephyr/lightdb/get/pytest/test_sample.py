import contextlib
import logging
import pytest
import time
import yaml
import datetime

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_lightdb_get(shell, device, credentials_file):
    # Delete counter in lightdb state

    await device.lightdb.delete("counter")

    # Read credentials

    with open(credentials_file, 'r') as f:
        credentials = yaml.safe_load(f)

    time.sleep(2)

    # Set credentials

    for setting in credentials['settings']:
        shell.exec_command(f"settings set {setting} \"{credentials['settings'][setting]}\"")

    shell._device.clear_buffer()
    shell._device.write('kernel reboot cold\n\n'.encode())

    # Wait for Golioth connection

    shell._device.readlines_until(regex=".*Golioth CoAP client connected", timeout=30.0)

    # Verify lightdb reads

    shell._device.readlines_until(regex=".*Failed to get counter \(async\): 0", timeout=10.0)

    await device.lightdb.set("counter", 13)

    shell._device.readlines_until(regex=".*Counter \(sync\): 13", timeout=10.0)

    await device.lightdb.set("counter", 27)

    shell._device.readlines_until(regex=".*22 63 6f 75 6e 74 65 72  22 3a 32 37             |\"counter \":27",
                                  timeout=10.0)

    await device.lightdb.set("counter", 42)

    shell._device.readlines_until(regex=".*Counter \(async\): 42", timeout=10.0)
