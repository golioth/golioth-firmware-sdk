# Copyright (c) 2024 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import logging
import pytest
import datetime
import trio

LOGGER = logging.getLogger(__name__)

LOG_FETCH_POLLING_LIMIT = datetime.timedelta(seconds=60)
LOG_EXPECTED_COUNT = 6 # we expect 4 hello messages plus waiting for conn and client connected

pytestmark = pytest.mark.anyio

async def test_hello(board, device):
    # Set Golioth credential
    golioth_cred = (await device.credentials.list())[0]
    await board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Record timestamp and wait for fourth hello message
    start = datetime.datetime.now(datetime.UTC)
    await board.wait_for_regex_in_line('.*Sending hello! 3', timeout_s=90.0)

    # Fetch logs

    log_fetch_start = datetime.datetime.now(datetime.UTC)

    while True:
        end = datetime.datetime.now(datetime.UTC)

        logs = await device.get_logs(
            {
                "start": start.strftime("%Y-%m-%dT%H:%M:%S.%fZ"),
                "end": end.strftime("%Y-%m-%dT%H:%M:%S.%fZ"),
                "module": "hello",
            }
        )

        if len(logs) >= LOG_EXPECTED_COUNT:
            break

        assert (
            LOG_FETCH_POLLING_LIMIT
            > datetime.datetime.now(datetime.UTC) - log_fetch_start
        )

        await trio.sleep(1)

    # Test logs received from server
    LOGGER.info("Searching log messages from end to start:")
    test_idx = 2
    test_hits = 0
    for m in reversed(logs):

        if m.message == f"Sending hello! {test_idx}":
            LOGGER.info("### MATCH FOUND! ---> {0}".format(m.message))
            test_hits += 1
            test_idx -= 1
            if test_idx < 0:
                break
        else:
            LOGGER.info(m.message)

    assert test_hits == 3, 'Unable to find all Hello messages on server'
