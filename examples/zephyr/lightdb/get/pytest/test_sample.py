import contextlib
import logging
import pytest
import time
import yaml
import datetime

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_lightdb_get(shell, device, wifi_ssid, wifi_psk):
    # Delete counter in lightdb state

    await device.lightdb.delete("counter")

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

    shell._device.readlines_until(regex=".*Golioth CoAP client connected", timeout=90.0)

    # Verify lightdb reads

    shell._device.readlines_until(regex=".*Failed to get counter \(async\): 0", timeout=10.0)

    await device.lightdb.set("counter", 13)

    shell._device.readlines_until(regex=".*Counter \(sync\): 13", timeout=10.0)

    await device.lightdb.set("counter", 27)

    shell._device.readlines_until(regex=".*22 63 6f 75 6e 74 65 72  22 3a 32 37             |\"counter \":27",
                                  timeout=10.0)

    await device.lightdb.set("counter", 99)

    shell._device.readlines_until(regex=".*Counter \(CBOR async\): 99", timeout=10.0)

    await device.lightdb.set("counter", 42)

    shell._device.readlines_until(regex=".*Counter \(async\): 42", timeout=10.0)
