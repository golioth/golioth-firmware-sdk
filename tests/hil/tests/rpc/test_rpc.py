import pytest
import time
from golioth import RPCResultError, RPCStatusCode, RPCTimeout

pytestmark = pytest.mark.anyio

@pytest.fixture(autouse=True, scope="module")
async def setup(project, board, device):
    # Set Golioth credentials
    golioth_cred = (await device.credentials.list())[0]
    board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Confirm connection to Golioth
    assert None != board.wait_for_regex_in_line('Golioth CoAP client connected', timeout_s=120)

##### Tests #####

async def test_not_registered(board, device):
    with pytest.raises(RPCResultError) as e:
        await device.rpc.not_registered([])

    assert e.value.status_code == RPCStatusCode.NOT_FOUND

async def test_no_args(board, device):
    await device.rpc.no_response()

    assert None != board.wait_for_regex_in_line('Received no_response with 0 args', timeout_s=10)

async def test_one_arg(board, device):
    await device.rpc.no_response("foo")

    assert None != board.wait_for_regex_in_line('Received no_response with 1 args', timeout_s=10)

async def test_many_args(board, device):
    await device.rpc.no_response(1,2,3,4,5,6,7,8,9)

    assert None != board.wait_for_regex_in_line('Received no_response with 9 args', timeout_s=10)

async def test_int_return(board, device):
    rsp = await device.rpc.basic_return_type("int")

    assert rsp['value'] == 42

async def test_string_return(board, device):
    rsp = await device.rpc.basic_return_type("string")

    assert rsp['value'] == "foo"

async def test_float_return(board, device):
    rsp = await device.rpc.basic_return_type("float")

    assert rsp['value'] == 12.34

async def test_bool_return(board, device):
    rsp = await device.rpc.basic_return_type("bool")

    assert rsp['value'] == True

async def test_object_return(board, device):
    rsp = await device.rpc.object_return_type()

    assert rsp['foo']['bar']['baz'] == 357

async def test_malformed_response(board, device):
    with pytest.raises(RPCTimeout):
        rsp = await device.rpc.malformed_response()

async def test_invalid_arg(board, device):
    with pytest.raises(RPCResultError) as e:
        rsp = await device.rpc.basic_return_type("foo")

    assert e.value.status_code == RPCStatusCode.INVALID_ARGUMENT

async def test_observation_reestablishment(board, device):
    rsp = await device.rpc.disconnect()

    # Confirm re-connection to Golioth
    assert None != board.wait_for_regex_in_line('Golioth CoAP client connected', timeout_s=120)

    rsp = await device.rpc.basic_return_type("int")

    assert rsp['value'] == 42
