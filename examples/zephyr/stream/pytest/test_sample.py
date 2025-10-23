import datetime
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

    lower_temp = 20.0
    upper_temp = 21.5

    start = datetime.datetime.now(datetime.UTC)
    shell._device.readlines_until(
        regex=".*Sending temperature {}.".format(str(upper_temp)), timeout=90.0
    )

    await trio.sleep(2) # Give stream time to hit server
    end = datetime.datetime.now(datetime.UTC)

    stream_data = await device.stream.get(path = 'temp',
                                          start = start.strftime('%Y-%m-%dT%H:%M:%S.%fZ'),
                                          end = end.strftime('%Y-%m-%dT%H:%M:%S.%fZ'))

    temp = lower_temp
    for value in reversed(stream_data["list"]):
        LOGGER.info("ts: {0}, temp: {1}".format(value["time"], value["data"]["temp"]))
        assert (value["data"]["temp"] == temp)

        if temp == upper_temp:
            break

        temp += 0.5
