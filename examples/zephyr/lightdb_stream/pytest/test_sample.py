import contextlib
import logging
import pytest
import time
import yaml
import datetime

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_lightdb_stream(shell, device, wifi_ssid, wifi_psk):
    time.sleep(2)

    # Set Golioth credential

    golioth_cred = (await device.credentials.list())[0]
    shell.exec_command(f"settings set golioth/psk-id {golioth_cred.identity}")
    shell.exec_command(f"settings set golioth/psk {golioth_cred.key}")

    # Set WiFi credential

    shell.exec_command(f"settings set wifi/ssid \"{wifi_ssid}\"")
    shell.exec_command(f"settings set wifi/psk \"{wifi_psk}\"")

    # Verify temp messages

    temp = 20.0
    async with contextlib.aclosing(device.stream.iter()) as stream_iter:
        async for value in stream_iter:
            LOGGER.info("ts: {0}, temp: {1}".format(value['timestamp'], value['data']['sensor']['temp']))
            assert (value["data"]["sensor"]["temp"] == temp)
            temp += 0.5
            if temp == 21.5:
                break
