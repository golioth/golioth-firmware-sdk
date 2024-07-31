# Copyright (c) 2024 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import logging
import pytest
import time
import datetime
import pprint

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_fw_update(board, project, device, fw_info, release):

    # Wait for app to start running or 10 seconds to pass so runtime settings are ready.

    try:
        board.wait_for_regex_in_line('.*Start FW Update sample.', timeout_s=10.0)
    except:
        pass

    # Set Golioth credential
    golioth_cred = (await device.credentials.list())[0]
    board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Wait for first manifest to arrive (no update expected)

    board.wait_for_regex_in_line('.*Nothing to do.', timeout_s=90.0)

    # Rollout the release

    await project.releases.rollout_set(release.id, True)

    # Monitor block download and watch for reboot after update

    board.wait_for_regex_in_line('.*Received block.', timeout_s=90.0)
    LOGGER.info("Block download has begun!")

    board.wait_for_regex_in_line('.*Rebooting into new image.', timeout_s=600.0)
    LOGGER.info("Download complete, restarting to perform update.")

    # Test for board to run new firmware and report to Golioth

    board.wait_for_regex_in_line('.*Current firmware version: main - 255.8.9.',
                                  timeout_s=120.0)
    LOGGER.info("Device reported expected update version")

    board.wait_for_regex_in_line('.*Nothing to do.', timeout_s=30.0)
    LOGGER.info("Device received manifest confirming successful upgrade.")

    # Check cloud for reported firmware version

    device_check = await project.device_by_name(device.name)

    # Print the metadata about firmware updates
    print("Firmware update Metadata:")
    pprint.pprint(device_check.info.get('metadata', {}))

    # Access the specific metadata related to firmware updates
    firmware_update_metadata = device_check.info['metadata']['update']

    print("fw_info:")
    pprint.pprint(fw_info)

    d_package = device_check.info['metadata']['update']['main']['package']
    d_version = device_check.info['metadata']['update']['main']['version']

    assert d_package == fw_info['package'], f'Expected firmware package "{fw_info["package"]}" but got "%s"'.format(d_package)
    assert d_version == fw_info['version'], f'Expected firmware version "{fw_info["version"]}" but got "%s"'.format(d_version)
