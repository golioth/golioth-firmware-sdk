# Copyright (c) 2025 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import datetime

from twister_harness.device.device_adapter import DeviceAdapter
from twister_harness.helpers.shell import Shell

import pytest
import trio

pytestmark = pytest.mark.anyio


async def test_location(shell: Shell, dut: DeviceAdapter, device):
    # Set Golioth credential

    golioth_cred = (await device.credentials.list())[0]
    shell.exec_command(f"settings set golioth/psk-id {golioth_cred.identity}")
    shell.exec_command(f"settings set golioth/psk {golioth_cred.key}")


    # Wait for Golioth connection

    start = datetime.datetime.now(datetime.UTC)
    dut.readlines_until(regex=".*Golioth CoAP client connected", timeout=90.0)

    # Wait for device to send location

    dut.readlines_until(regex=".*Successfully streamed network data", timeout=90.0)

    # Verify network info data

    await trio.sleep(2) # Give stream time to hit server
    end = datetime.datetime.now(datetime.UTC)

    stream_data = await device.stream.get(path = '',
                                          start = start.strftime('%Y-%m-%dT%H:%M:%S.%fZ'),
                                          end = end.strftime('%Y-%m-%dT%H:%M:%S.%fZ'))

    for value in reversed(stream_data["list"]):
        assert "cell" in value["data"] or "wifi" in value["data"]
        break
