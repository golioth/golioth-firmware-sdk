# Copyright (c) 2024 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

from golioth import RPCStatusCode
import logging
import pytest

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_logging(board, device):
    # Set Golioth credential
    golioth_cred = (await device.credentials.list())[0]
    board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Reset board
    board.reset()

    # Wait for device to reboot and connect
    board.wait_for_regex_in_line('.*rpc: Golioth client connected', timeout_s=90.0)

    # Test successful RPC
    result = await device.rpc.call("multiply", [ 7, 6 ])
    LOGGER.info("### Received: {0} Expected: {1}".format(result['value'],  42))
    assert result['value'] == 42, "Didn't receive correct value"

    # Test successful RPC using float

    result = await device.rpc.call("multiply", [ 11.4, 93.81 ])
    LOGGER.info("### Received: {0} Expected: {1}".format(result['value'],  1069.434))
    assert result['value'] == 1069.434, "Didn't receive correct float"

    # Test invalid argument RPC

    error_code = None
    try:
        result = await device.rpc.call("multiply", [ 6, 'J' ])
    except Exception as e:
        error_code = e
        pass
    LOGGER.info("### Received: {0} Expected: {1}".format(error_code.status_code,  RPCStatusCode.INVALID_ARGUMENT))
    assert error_code.status_code == RPCStatusCode.INVALID_ARGUMENT, "Didn't receive correct error code"
