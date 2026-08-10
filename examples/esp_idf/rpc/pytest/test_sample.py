# Copyright (c) 2024 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

from golioth import RPCResultError, RPCStatusCode, RPCTimeout
import logging
import pytest

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_rpc(board, device):
    # Set Golioth credential
    golioth_cred = (await device.credentials.list())[0]
    await board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Wait for device to reboot and connect
    await board.wait_for_regex_in_line('.*RPC observation established', timeout_s=90.0)

    # Test successful RPC
    result = await device.rpc.call("multiply", [ 7, 6 ])
    LOGGER.info("### Received: {0} Expected: {1}".format(result['value'],  42))
    assert result['value'] == 42, "Didn't receive correct value"

    # Test successful RPC using float

    result = await device.rpc.call("multiply", [ 11.4, 93.81 ])
    LOGGER.info("### Received: {0} Expected: {1}".format(result['value'],  1069.434))
    assert result['value'] == 1069.434, "Didn't receive correct float"

    # Test invalid argument RPC

    try:
        result = await device.rpc.call("multiply", [ 6, 'J' ])
    except RPCTimeout:
        assert False, "RPC with invalid args timed out"
    except RPCResultError as e:
        LOGGER.info("### Received: {0} Expected: {1}".format(e.status_code,  RPCStatusCode.INVALID_ARGUMENT))
        assert e.status_code == RPCStatusCode.INVALID_ARGUMENT, "Didn't receive correct error code"
    else:
        assert False, f"RPC with invalid args should have raised an error, but returned: {result}"
