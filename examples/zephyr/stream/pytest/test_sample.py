import contextlib
import logging

import pytest
import trio

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_stream(shell, device, wifi_ssid, wifi_psk):
    await trio.sleep(2)

    # Set Golioth credential

    golioth_cred = (await device.credentials.list())[0]
    shell.exec_command(f"settings set golioth/psk-id {golioth_cred.identity}")
    shell.exec_command(f"settings set golioth/psk {golioth_cred.key}")

    # Set WiFi credential

    if wifi_ssid is not None and wifi_psk is not None:
        shell.exec_command(f"wifi cred add -k 1 -s \"{wifi_ssid}\" -p \"{wifi_psk}\"")
        shell.exec_command("wifi cred auto_connect")

    # Verify temp messages

    temp = 20.0
    async with contextlib.aclosing(device.stream.iter()) as stream_iter:
        async for value in stream_iter:
            LOGGER.info("ts: {0}, temp: {1}".format(value['timestamp'], value['data']['temp']))
            assert (value["data"]["temp"] == temp)
            temp += 0.5
            if temp == 22.0:
                break
