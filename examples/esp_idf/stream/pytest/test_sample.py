# Copyright (c) 2024 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import contextlib
import logging

import pytest
import trio

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_stream(board, device):
    await trio.sleep(2)

    # Set Golioth credential
    golioth_cred = (await device.credentials.list())[0]
    board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Wait for device to connect
    board.wait_for_regex_in_line('.*Golioth client connected', timeout_s=30.0)

    # Verify temp messages

    lower_temp = 20.0
    upper_temp = 21.5
    async with contextlib.aclosing(device.stream.iter()) as stream_iter:
        async for value in stream_iter:
            LOGGER.info("ts: {0}, temp: {1}".format(value['timestamp'], value['data']['temp']))
            assert (value["data"]["temp"] >= lower_temp and value["data"]["temp"] <= upper_temp)
            if value["data"]["temp"] == upper_temp:
                break
