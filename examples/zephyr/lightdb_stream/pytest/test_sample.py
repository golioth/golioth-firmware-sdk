from golioth import Client, LogLevel, LogEntry
import contextlib
import logging
import pytest
import time
import yaml
import datetime

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_lightdb_stream(shell, api_key, device_name, credentials_file):

    # Connect to Golioth and get device object

    client = Client(api_url = "https://api.golioth.dev",
                    api_key = api_key)
    project = (await client.get_projects())[0]
    device = await project.device_by_name(device_name)

    # Read credentials

    with open(credentials_file, 'r') as f:
        credentials = yaml.safe_load(f)

    time.sleep(2)

    # Set credentials

    for setting in credentials['settings']:
        shell.exec_command(f"settings set {setting} \"{credentials['settings'][setting]}\"")

    shell._device.clear_buffer()
    shell._device.write('kernel reboot cold\n\n'.encode())

    # Verify temp messages

    temp = 20.0
    async with contextlib.aclosing(device.stream.iter()) as stream_iter:
        async for value in stream_iter:
            LOGGER.info("ts: {0}, temp: {1}".format(value['timestamp'], value['data']['sensor']['temp']))
            assert (value["data"]["sensor"]["temp"] == temp)
            temp += 0.5
            if temp == 21.5:
                break
