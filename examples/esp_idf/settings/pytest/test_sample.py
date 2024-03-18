# Copyright (c) 2024 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import logging
import pytest
import pprint
import time
import yaml

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_settings(board, project, device):
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
    board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Reset board
    board.reset()

    # Wait for device to reboot and connect
    board.wait_for_regex_in_line(r'.*Golioth client connected', timeout_s=30.0)

    # Set device-level setting

    await device.settings.set('LOOP_DELAY_S', 5)

    board.wait_for_regex_in_line(r'.*Setting loop delay to 5 s', timeout_s=5.0)
