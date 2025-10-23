# Copyright (c) 2024 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import datetime
import logging

import pytest
import trio

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_stream(board, device):
    await trio.sleep(2)

    # Set Golioth credential
    golioth_cred = (await device.credentials.list())[0]
    await board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Wait for device to connect
    await board.wait_for_regex_in_line('.*Golioth client connected', timeout_s=30.0)

    # Verify temp messages

    lower_temp = 20.0
    upper_temp = 21.5

    start = datetime.datetime.now(datetime.UTC)
    await board.wait_for_regex_in_line(
        ".*Sending temperature {}.".format(str(upper_temp)), timeout_s=90.0
    )

    await trio.sleep(2) # Give stream time to hit server
    end = datetime.datetime.now(datetime.UTC)

    stream_data = await device.stream.get(path = 'temp',
                                          start = start.strftime('%Y-%m-%dT%H:%M:%S.%fZ'),
                                          end = end.strftime('%Y-%m-%dT%H:%M:%S.%fZ'))

    temp = lower_temp
    for value in reversed(stream_data["list"]):
        LOGGER.info("ts: {0}, temp: {1}".format(value["time"], value["data"]["temp"]))
        assert (value["data"]["temp"] == temp)

        if temp == upper_temp:
            break

        temp += 0.5
