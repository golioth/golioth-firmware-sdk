import logging
import pytest
import pprint
import time
import yaml

pytestmark = pytest.mark.anyio

async def test_settings(shell, project, device, wifi_ssid, wifi_psk):
    # Delete any existing device-level settings

    settings = await device.settings.get_all()
    for setting in settings:
        if 'deviceId' in setting:
            await device.settings.delete(setting['key'])

    # Ensure the project-level setting exists

    await project.settings.set('LOOP_DELAY_S', 1)

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

    shell._device.readlines_until(regex=".*Setting loop delay to 1 s", timeout=90.0)

    # Set device-level setting

    await device.settings.set('LOOP_DELAY_S', 5)

    shell._device.readlines_until(regex=".*Setting loop delay to 5 s", timeout=5.0)
