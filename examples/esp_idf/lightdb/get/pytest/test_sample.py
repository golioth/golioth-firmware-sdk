# Copyright (c) 2024 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import logging

import pytest
import trio

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_lightdb_get(board, device):
    # Delete counter in lightdb state

    await device.lightdb.delete("counter")

    await trio.sleep(2)

    # Set Golioth credential
    golioth_cred = (await device.credentials.list())[0]
    await board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Wait for device to connect
    await board.wait_for_regex_in_line(r'.*Golioth client connected', timeout_s=30.0)

    # Verify lightdb reads

    await board.wait_for_regex_in_line(r'.*Failed to get counter \(async\): 0', timeout_s=10.0)

    await device.lightdb.set("counter", 13)

    await board.wait_for_regex_in_line(r'.*Counter \(sync\): 13', timeout_s=10.0)

    await device.lightdb.set("counter", 27)

    await board.wait_for_regex_in_line(r'.*7b 22 63 6f 75 6e 74 65  72 22 3a 32 37 7d       |{\"counte r\":27}',
                                  timeout_s=10.0)

    await device.lightdb.set("counter", 99)

    await board.wait_for_regex_in_line(r'.*Counter \(CBOR async\): 99', timeout_s=10.0)

    await device.lightdb.set("counter", 42)

    await board.wait_for_regex_in_line(r'.*Counter \(async\): 42', timeout_s=10.0)
