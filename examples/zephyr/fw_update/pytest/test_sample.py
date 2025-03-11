import logging
import pytest

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_fw_update(shell, project, device, wifi_ssid, wifi_psk, fw_info, release):

    # Wait for app to start running or 10 seconds to pass so runtime settings are ready.

    try:
        shell._device.readlines_until(regex=".*Start FW Update sample.", timeout=10.0)
    except:
        pass

    # Set Golioth credential

    golioth_cred = (await device.credentials.list())[0]
    shell.exec_command(f"settings set golioth/psk-id {golioth_cred.identity}")
    shell.exec_command(f"settings set golioth/psk {golioth_cred.key}")

    # Set WiFi credential

    if wifi_ssid is not None:
        shell.exec_command(f"settings set wifi/ssid \"{wifi_ssid}\"")

    if wifi_psk is not None:
        shell.exec_command(f"settings set wifi/psk \"{wifi_psk}\"")

    # Wait for first manifest to arrive (no update expected)

    shell._device.readlines_until(regex=".*Nothing to do.", timeout=90.0)


    # Rollout the release

    await project.releases.rollout_set(release.id, True)

    # Monitor block download and watch for reboot after update

    shell._device.readlines_until(regex=".*Received block.", timeout=90.0)
    LOGGER.info("Block download has begun!")

    shell._device.readlines_until(regex=".*Rebooting into new image.", timeout=600.0)
    LOGGER.info("Download complete, restarting to perform update.")

    # Test for board to run new firmware and report to Golioth

    shell._device.readlines_until(regex=f".*Current firmware version: {fw_info['package']} - {fw_info['version']}.",
                                  timeout=120.0)
    LOGGER.info("Device reported expected update version")

    shell._device.readlines_until(regex=".*Nothing to do.", timeout=30.0)
    LOGGER.info("Device received manifest confirming successful upgrade.")

    # Check cloud for reported firmware version

    device_check = await project.device_by_name(device.name)

    d_package = device_check.info['metadata']['update']['main']['package']
    d_version = device_check.info['metadata']['update']['main']['version']

    assert d_package == fw_info['package'], f'Expected firmware package "{fw_info["package"]}" but got "%s"'.format(d_package)
    assert d_version == fw_info['version'], f'Expected firmware version "{fw_info["version"]}" but got "%s"'.format(d_version)
