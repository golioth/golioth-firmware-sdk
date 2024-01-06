import logging
import pytest
import pprint
import time
import yaml

pytestmark = pytest.mark.anyio

async def test_settings(shell, project, device, credentials_file):
    # Delete any existing device-level settings

    settings = await device.settings.get_all()
    for setting in settings:
        if 'deviceId' in setting:
            await device.settings.delete(setting['key'])

    # Ensure the project-level setting exists

    await project.settings.set('LOOP_DELAY_S', 1)

    # Read credentials

    with open(credentials_file, 'r') as f:
        credentials = yaml.safe_load(f)

    time.sleep(2)

    # Set Golioth credential

    golioth_cred = (await device.credentials.list())[0]
    shell.exec_command(f"settings set golioth/psk-id {golioth_cred.identity}")
    shell.exec_command(f"settings set golioth/psk {golioth_cred.key}")

    # Set WiFi credential

    for setting in credentials['settings']:
        if 'golioth' in setting:
            continue
        shell.exec_command(f"settings set {setting} \"{credentials['settings'][setting]}\"")

    shell._device.clear_buffer()
    shell._device.write('kernel reboot cold\n\n'.encode())

    shell._device.readlines_until(regex=".*Setting loop delay to 1 s", timeout=30.0)

    # Set device-level setting

    await device.settings.set('LOOP_DELAY_S', 5)

    shell._device.readlines_until(regex=".*Setting loop delay to 5 s", timeout=5.0)
