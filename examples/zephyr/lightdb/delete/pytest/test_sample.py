from golioth import Client, LogLevel, LogEntry
import contextlib
import logging
import pytest
import time
import yaml
import datetime

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_lightdb_delete(shell, api_key, device_name, credentials_file):

    # Connect to Golioth and get device object

    client = Client(api_url = "https://api.golioth.dev",
                    api_key = api_key)
    project = (await client.get_projects())[0]
    device = await project.device_by_name(device_name)

    # Set counter in lightdb state

    await device.lightdb.set("counter", 34)
    time.sleep(0.1)
    counter = await device.lightdb.get("counter")
    assert counter == 34

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

    # Verify lightdb delete (async)

    shell._device.readlines_until(regex=".*Counter deleted successfully", timeout=10.0)
    counter = await device.lightdb.get("counter")
    assert counter is None

    # Set and verify counter

    await device.lightdb.set("counter", 62)
    time.sleep(0.1)
    counter = await device.lightdb.get("counter")
    assert counter == 62

    # Verify lightdb delete (sync)

    shell._device.readlines_until(regex=".*Counter deleted successfully", timeout=10.0)
    counter = await device.lightdb.get("counter")
    assert counter is None
