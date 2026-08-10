import logging

import pytest
import trio

from golioth import RPCResultError, RPCStatusCode, RPCTimeout

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_rpc(shell, device, wifi_ssid, wifi_psk):
    await trio.sleep(2)

    # Set Golioth credential

    golioth_cred = (await device.credentials.list())[0]
    shell.exec_command(f"settings set golioth/psk-id {golioth_cred.identity}")
    shell.exec_command(f"settings set golioth/psk {golioth_cred.key}")

    # Set WiFi credential

    if wifi_ssid is not None and wifi_psk is not None:
        shell.exec_command(f"wifi cred add -k 1 -s \"{wifi_ssid}\" -p \"{wifi_psk}\"")
        shell.exec_command("wifi cred auto_connect")

    # Wait for device to reboot and connect

    shell._device.readlines_until(regex=".*RPC observation established", timeout=90.0)

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
