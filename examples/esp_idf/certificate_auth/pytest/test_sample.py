# Copyright (c) 2024 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import pytest

pytestmark = pytest.mark.anyio

async def test_certificate_auth(project, board, device_cert_name, certificate_cred):
    await board.wait_for_regex_in_line('.*Counter = 3', timeout_s=90.0)

    device = await project.device_by_name(device_cert_name)
    assert device.name == device_cert_name
