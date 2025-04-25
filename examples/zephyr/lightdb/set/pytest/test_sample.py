import logging

import pytest
import trio

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_lightdb_set(shell, device, wifi_ssid, wifi_psk):
    # Delete counter in lightdb state

    await device.lightdb.delete("counter")

    await trio.sleep(2)

    # Set Golioth credential

    golioth_cred = (await device.credentials.list())[0]
    shell.exec_command(f"settings write string golioth/psk-id {golioth_cred.identity}")
    shell.exec_command(f"settings write string golioth/psk {golioth_cred.key}")

    # Set WiFi credential

    if wifi_ssid is not None:
        shell.exec_command(f"settings write string wifi/ssid \"{wifi_ssid}\"")

    if wifi_psk is not None:
        shell.exec_command(f"settings write string wifi/psk \"{wifi_psk}\"")

    # Wait for Golioth connection

    shell._device.readlines_until(regex=".*Golioth CoAP client connected", timeout=90.0)

    # Verify lightdb writes

    for i in range(0,4):
        shell._device.readlines_until(regex=f".*Setting counter to {i}", timeout=10.0)
        shell._device.readlines_until(regex=f".*Counter successfully set", timeout=10.0)

        for _ in range(3):
            counter = await device.lightdb.get("counter")
            if counter == i:
                break
            await trio.sleep(1)

        assert counter == i
