# Copyright (c) 2025 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

from collections import namedtuple
import re

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

    # Verify position

    pattern = re.compile(r".* (?P<lon>\d+\.\d+) (?P<lat>\d+\.\d+) \((?P<acc>\d+)\)")
    Position = namedtuple('Position', ('lon', 'lat', 'acc'))

    positions = []
    for _ in range(3):
        lines = dut.readlines_until(regex=pattern, timeout=30.0)
        m = pattern.search(lines[-1])
        pos = Position(*[float(m[p]) for p in ['lon', 'lat', 'acc']])
        positions.append(pos)

    for pos in positions:
        assert pos.lon == pytest.approx(50.663974800, 1e-3)
        assert pos.lat == pytest.approx(17.942322850, 1e-3)
        assert pos.acc < 2000
