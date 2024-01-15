from golioth import Client
import logging
import pytest
import time
import yaml

LOGGER = logging.getLogger(__name__)

UPDATE_VERSION = '255.8.9'
UPDATE_PACKAGE = 'main'
API_URL = "https://api.golioth.io"
CI_PROJ_NAME = "firmware_ci"

pytestmark = pytest.mark.anyio

async def test_fw_update(shell, project, device, credentials_file, west_board):
    # Read credentials

    with open(credentials_file, 'r') as f:
        credentials = yaml.safe_load(f)

    time.sleep(2)

    # Add blueprint to device

    LOGGER.info(f"Board name: {west_board}")

    blueprint_id = await project.blueprints.get_id(west_board)
    await device.add_blueprint(blueprint_id)

    # Generate tag name and add to device

    tag_name = device.name.lower().replace('-','_')
    tag = await project.tags.create(tag_name)
    await device.add_tag(tag.id)

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

    # Wait for first manifest to arrive (no update expected)

    shell._device.readlines_until(regex=".*Nothing to do.", timeout=90.0)

    # Find Artifact that matches this device and desired update version

    artifact_id = None
    all_artifacts = await project.artifacts.get_all()
    for a in all_artifacts:
        if (a.blueprint == blueprint_id and
            a.version == UPDATE_VERSION and
            a.package == UPDATE_PACKAGE):
            artifact_id = a.id

    assert artifact_id != None, f'Unable to find Artifact {UPDATE_PACKAGE}-{UPDATE_VERSION} with blueprint: {west_board}'

    # Create release

    release = await project.releases.create([artifact_id], [], [tag.id], True)

    # Monitor block download and watch for reboot after update

    shell._device.readlines_until(regex=".*Downloading block index.", timeout=90.0)
    LOGGER.info("Block download has begun!")

    ##FIXME: remove these lines for now because the nRF9160 doesn't send this message
    ##       when this is fixed, the timeout for "Current firmware" should be changed back to 90.0
    #
    # shell._device.readlines_until(regex=".*Rebooting into new image.", timeout=600.0)
    # LOGGER.info("Download complete, restarting to perform update.")

    shell._device.readlines_until(regex=".*Current firmware version: main - 255.8.9.", timeout=600.0)
    LOGGER.info("Device reported expected update version")

    shell._device.readlines_until(regex=".*Nothing to do.", timeout=30.0)
    LOGGER.info("Device received manifest confirming successful upgrade.")

    # Check cloud for reported firmware version

    device_check = await project.device_by_name(device.name)

    d_package = device_check.info['metadata']['update']['main']['package']
    d_version = device_check.info['metadata']['update']['main']['version']

    assert d_package == 'main', f'Expected firmware package "main" but got "%s"'.format(d_package)
    assert d_version == '255.8.9', f'Expected firmware version "255.8.9" but got "%s"'.format(d_version)

    # Clean Up

    await project.releases.delete(release.id)
    await device.remove_tag(tag.id)
    await project.tags.delete(tag.id)
