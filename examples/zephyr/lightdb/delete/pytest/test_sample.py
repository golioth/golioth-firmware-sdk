import logging

import pytest
import trio

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def counter_set_and_verify(device, counter_desired: int):
    for _ in range(3):
        await device.lightdb.set("counter", counter_desired)
        counter_reported = await device.lightdb.get("counter")
        if counter_reported == counter_desired:
            break

        await trio.sleep(1)

    assert counter_reported == counter_desired

async def test_lightdb_delete(shell, device, wifi_ssid, wifi_psk):
    # Set counter in lightdb state

    await counter_set_and_verify(device, 34)

    await trio.sleep(2)

    # Set Golioth credential

    golioth_cred = (await device.credentials.list())[0]
    shell.exec_command(f"settings set golioth/psk-id {golioth_cred.identity}")
    shell.exec_command(f"settings set golioth/psk {golioth_cred.key}")

    # Set WiFi credential

    if wifi_ssid is not None and wifi_psk is not None:
        shell.exec_command(f"wifi cred add -k 1 -s \"{wifi_ssid}\" -p \"{wifi_psk}\"")
        shell.exec_command("wifi cred auto_connect")

    # Wait for Golioth connection

    shell._device.readlines_until(regex=".*Golioth CoAP client connected", timeout=90.0)

    # Verify lightdb delete

    shell._device.readlines_until(regex=".*Counter deleted successfully", timeout=10.0)
    await trio.sleep(2)
    counter = await device.lightdb.get("counter")
    assert counter is None
