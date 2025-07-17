# Copyright (c) 2025 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import contextlib

from twister_harness.device.device_adapter import DeviceAdapter
from twister_harness.helpers.shell import Shell

import pytest

pytestmark = pytest.mark.anyio


async def test_location(shell: Shell, dut: DeviceAdapter, device):
    # Set Golioth credential

    golioth_cred = (await device.credentials.list())[0]
    shell.exec_command(f"settings set golioth/psk-id {golioth_cred.identity}")
    shell.exec_command(f"settings set golioth/psk {golioth_cred.key}")

    # Wait for Golioth connection

    dut.readlines_until(regex=".*Golioth CoAP client connected", timeout=90.0)

    # Verify network info data
    async with contextlib.aclosing(device.stream.iter()) as stream_iter:
        async for value in stream_iter:
            assert "cell" in value["data"] or "wifi" in value["data"]
            break
