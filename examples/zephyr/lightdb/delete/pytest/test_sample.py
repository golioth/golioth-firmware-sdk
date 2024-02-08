import contextlib
import logging
import pytest
import time
import yaml
import datetime

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_lightdb_delete(shell, device, wifi_ssid, wifi_psk):
    # Set counter in lightdb state

    await device.lightdb.set("counter", 34)
    counter = await device.lightdb.get("counter")
    assert counter == 34

    time.sleep(2)

    # Set Golioth credential

    golioth_cred = (await device.credentials.list())[0]
    shell.exec_command(f"settings set golioth/psk-id {golioth_cred.identity}")
    shell.exec_command(f"settings set golioth/psk {golioth_cred.key}")

    # Set WiFi credential

    shell.exec_command(f"settings set wifi/ssid \"{wifi_ssid}\"")
    shell.exec_command(f"settings set wifi/psk \"{wifi_psk}\"")

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
    counter = await device.lightdb.get("counter")
    assert counter == 62

    # Verify lightdb delete (sync)

    shell._device.readlines_until(regex=".*Counter deleted successfully", timeout=10.0)
    counter = await device.lightdb.get("counter")
    assert counter is None
