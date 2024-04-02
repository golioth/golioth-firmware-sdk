import contextlib
import logging
import pytest
import time
import yaml
import datetime

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_lightdb_observe(shell, device, wifi_ssid, wifi_psk):
    # Delete counter in lightdb state

    await device.lightdb.delete("counter")

    time.sleep(2)

    # Set Golioth credential

    golioth_cred = (await device.credentials.list())[0]
    shell.exec_command(f"settings set golioth/psk-id {golioth_cred.identity}")
    shell.exec_command(f"settings set golioth/psk {golioth_cred.key}")

    # Set WiFi credential

    if wifi_ssid is not None:
        shell.exec_command(f"settings set wifi/ssid \"{wifi_ssid}\"")

    if wifi_psk is not None:
        shell.exec_command(f"settings set wifi/psk \"{wifi_psk}\"")

    # Wait for Golioth connection

    shell._device.readlines_until(regex=".*Golioth CoAP client connected", timeout=90.0)

    # Verify lightdb observe

    shell._device.readlines_until(regex=".*lightdb_observe: Counter \(async\)", timeout=10.0)
    shell._device.readlines_until(regex=".*6e 75 6c 6c\s+|null", timeout=1.0)

    await device.lightdb.set("counter", 87)

    shell._device.readlines_until(regex=".*lightdb_observe: Counter \(async\)", timeout=5.0)
    shell._device.readlines_until(regex=".*38 37\s+|87", timeout=1.0)
