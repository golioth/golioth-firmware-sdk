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

async def test_lightdb_delete(board, device):
    # Set counter in lightdb state

    await device.lightdb.set("counter", 34)
    counter = await device.lightdb.get("counter")
    assert counter == 34

    time.sleep(2)

    # Set Golioth credential
    golioth_cred = (await device.credentials.list())[0]
    board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Reset board
    board.reset()

    # Wait for device to reboot and connect
    board.wait_for_regex_in_line(r'.*Golioth client connected', timeout_s=30.0)

    # Verify lightdb delete (async)

    board.wait_for_regex_in_line(r'.*Counter deleted successfully', timeout_s=10.0)
    counter = await device.lightdb.get("counter")
    assert counter is None

    # Set and verify counter

    await device.lightdb.set("counter", 62)
    counter = await device.lightdb.get("counter")
    assert counter == 62

    # Verify lightdb delete (sync)

    board.wait_for_regex_in_line(r'.*Counter deleted successfully', timeout_s=10.0)
    counter = await device.lightdb.get("counter")
    assert counter is None
