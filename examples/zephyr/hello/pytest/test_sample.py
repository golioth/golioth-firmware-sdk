import datetime
import logging
import pytest
import trio

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_hello(shell, device, wifi_ssid, wifi_psk):
    await trio.sleep(2)

    # Set Golioth credential

    golioth_cred = (await device.credentials.list())[0]
    shell.exec_command(f"settings set golioth/psk-id {golioth_cred.identity}")
    shell.exec_command(f"settings set golioth/psk {golioth_cred.key}")

    # Set WiFi credential

    if wifi_ssid is not None and wifi_psk is not None:
        shell.exec_command(f"wifi cred add -k 1 -s \"{wifi_ssid}\" -p \"{wifi_psk}\"")
        shell.exec_command("wifi cred auto_connect")

    # Record timestamp and wait for fourth hello message

    start = datetime.datetime.now(datetime.UTC)
    shell._device.readlines_until(regex=".*Sending hello! 3", timeout=110.0)

    # Check logs for hello messages

    end = datetime.datetime.now(datetime.UTC)

    logs = await device.get_logs({'start': start.strftime('%Y-%m-%dT%H:%M:%S.%fZ'), 'end': end.strftime('%Y-%m-%dT%H:%M:%S.%fZ')})

    # Test logs received from server

    LOGGER.info("Searching log messages from end to start:")
    test_idx = 2
    test_hits = 0
    for m in reversed(logs):

        if m.message == f"Sending hello! {test_idx}":
            LOGGER.info("### MATCH FOUND! ---> {0}".format(m.message))
            test_hits += 1
            test_idx -= 1
            if test_idx < 0:
                break
        else:
            LOGGER.info(m.message)

    assert test_hits == 3, 'Unable to find all Hello messages on server'
