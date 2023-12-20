from golioth import Client, LogLevel, LogEntry
import contextlib
import logging
import pytest
import time
import yaml
import datetime

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_lightdb_observe(shell, api_key, device_name, credentials_file):

    # Connect to Golioth and get device object

    client = Client(api_url = "https://api.golioth.dev",
                    api_key = api_key)
    project = (await client.get_projects())[0]
    device = await project.device_by_name(device_name)

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

    # Verify lightdb observe

    shell._device.readlines_until(regex=".*lightdb_observe: Counter \(async\)", timeout=10.0)
    shell._device.readlines_until(regex=".*6e 75 6c 6c\s+|null", timeout=1.0)

    await device.lightdb.set("counter", 87)

    shell._device.readlines_until(regex=".*lightdb_observe: Counter \(async\)", timeout=2.0)
    shell._device.readlines_until(regex=".*38 37\s+|87", timeout=1.0)
