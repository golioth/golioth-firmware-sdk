# Copyright (c) 2024 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import logging
import re

import pytest
import trio

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_lightdb_observe(board, device):
    # Delete counter in lightdb state

    await device.lightdb.delete("counter")

    await trio.sleep(2)

    # Set Golioth credential
    golioth_cred = (await device.credentials.list())[0]
    board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Wait for device to connect
    board.wait_for_regex_in_line('.*Golioth client connected', timeout_s=30.0)

    await trio.sleep(2)

    pattern = re.compile(r'.*6e 75 6c 6c\s+\|null')
    success_pattern = board.wait_for_regex_in_line(pattern, timeout_s=10.0)
    if success_pattern:
        print("Success for pattern '6e 75 6c 6c |null'")

    await device.lightdb.set("counter", 87)

    await trio.sleep(2)

    pattern = re.compile(r'.*38 37\s+\|87')
    success_pattern = board.wait_for_regex_in_line(pattern, timeout_s=10.0)
    if success_pattern:
        print("Success for pattern '38 37 |87'")
