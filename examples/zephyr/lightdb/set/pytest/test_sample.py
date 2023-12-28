import contextlib
import logging
import pytest
import time
import yaml
import datetime

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_lightdb_set(shell, device, credentials_file):
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

    # Verify lightdb writes

    for i in range(0,4):
        shell._device.readlines_until(regex=f".*Setting counter to {i}", timeout=10.0)
        counter = await device.lightdb.get("counter")
        assert counter == i
