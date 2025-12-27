import logging

import pytest
import trio

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_lightdb_get(shell, device, wifi_ssid, wifi_psk):
    # Delete counter in lightdb state

    await device.lightdb.delete("counter")

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

    # Verify lightdb reads

    shell._device.readlines_until(regex=".*Failed to get counter: 0", timeout=10.0)

    await device.lightdb.set("counter", 27)

    shell._device.readlines_until(regex="LightDB JSON: {\"counter\":27}", timeout=10.0)

    await device.lightdb.set("counter", 99)

    shell._device.readlines_until(regex=".*Counter \(CBOR\): 99", timeout=10.0)

    await device.lightdb.set("counter", 42)

    shell._device.readlines_until(regex=".*Counter: 42", timeout=10.0)
