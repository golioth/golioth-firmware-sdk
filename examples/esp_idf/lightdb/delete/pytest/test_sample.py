# Copyright (c) 2024 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import logging
import pytest
import trio

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def counter_set_and_verify(device, counter_desired: int):
    for _ in range(3):
        await device.lightdb.set("counter", counter_desired)
        counter_reported = await device.lightdb.get("counter")
        if counter_reported == counter_desired:
            break

        await trio.sleep(1)

    assert counter_reported == counter_desired

async def test_lightdb_delete(board, device):
    # Set counter in lightdb state

    await counter_set_and_verify(device, 34)

    await trio.sleep(2)

    # Set Golioth credential
    golioth_cred = (await device.credentials.list())[0]
    await board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Wait for device to connect
    await board.wait_for_regex_in_line(r'.*Golioth client connected', timeout_s=30.0)

    # Verify lightdb delete

    await board.wait_for_regex_in_line(r'.*Counter deleted successfully', timeout_s=10.0)
    counter = await device.lightdb.get("counter")
    assert counter is None
