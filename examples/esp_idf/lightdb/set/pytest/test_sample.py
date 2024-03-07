# Copyright (c) 2024 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import contextlib
import logging
import pytest
import time
import yaml
import datetime
import re

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_lightdb_set(board, device):
    # Delete counter in lightdb state

    await device.lightdb.delete("counter")

    time.sleep(2)

    # Set Golioth credential
    golioth_cred = (await device.credentials.list())[0]
    board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Reset board
    board.reset()

    # Wait for device to reboot and connect
    board.wait_for_regex_in_line('.*Golioth client connected', timeout_s=30.0)

    # Verify lightdb writes

    for i in range(0,4):
        pattern = re.compile(fr"Setting counter to {i}")
        board.wait_for_regex_in_line(pattern, timeout_s=30.0)
        time.sleep(1)
        counter = await device.lightdb.get("counter")
        assert counter == i
