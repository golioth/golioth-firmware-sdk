# Copyright (c) 2024 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import pytest

pytestmark = pytest.mark.anyio

async def test_certificate_auth(certificate_name, root_certificate):
    print(f"Device name: {certificate_name}")
    print(f"Root certificate: {root_certificate}")
