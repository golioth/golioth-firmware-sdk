import pytest
import trio
from enum import Enum

pytestmark = pytest.mark.anyio

@pytest.fixture(autouse=True, scope="module")
async def setup(project, board, device):
    # Set Golioth credentials
    golioth_cred = (await device.credentials.list())[0]
    await board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Setup data for replication
    await device.lightdb.delete('hil/lightdb')

    await device.lightdb.set('hil/lightdb/desired/true', True)
    await device.lightdb.set('hil/lightdb/desired/false', False)
    await device.lightdb.set('hil/lightdb/desired/int', 13)
    await device.lightdb.set('hil/lightdb/desired/float', -234.25)
    await device.lightdb.set('hil/lightdb/desired/str', '15')
    await device.lightdb.set('hil/lightdb/desired/obj', {'c': '26', 'd': -12})

    await device.lightdb.set('hil/lightdb/to_delete/sync/true', True)
    await device.lightdb.set('hil/lightdb/to_delete/sync/false', False)
    await device.lightdb.set('hil/lightdb/to_delete/sync/int', 14)
    await device.lightdb.set('hil/lightdb/to_delete/sync/str', '16')
    await device.lightdb.set('hil/lightdb/to_delete/sync/obj', {'a': '16', 'b': 12})

    await device.lightdb.set('hil/lightdb/to_delete/async/true', True)
    await device.lightdb.set('hil/lightdb/to_delete/async/false', False)
    await device.lightdb.set('hil/lightdb/to_delete/async/int', 14)
    await device.lightdb.set('hil/lightdb/to_delete/async/str', '16')
    await device.lightdb.set('hil/lightdb/to_delete/async/obj', {'a': '16', 'b': 12})

    await device.lightdb.set('hil/lightdb/observed/int', 0)
    await device.lightdb.set('hil/lightdb/observed/cbor/int', 0)

    # Mark test data ready
    await device.lightdb.set('hil/lightdb/desired/ready', True)

    # Confirm connection to Golioth
    assert None != await board.wait_for_regex_in_line('Golioth CoAP client connected', timeout_s=120)

    # Give some time to replicate desired state
    assert None != await board.wait_for_regex_in_line('LightDB State reported data is ready', timeout_s=30)

    # Give enough time for Golioth backend to propagate data
    await trio.sleep(5)

    yield

    await device.lightdb.delete('hil/lightdb')

##### Tests #####

async def wait_for_lightdb(device):
    # Retry until values are available
    retry_count = 3
    while retry_count:
        if await device.lightdb.get('hil/lightdb/reported/sync/true') != None:
            return;
        retry_count -= 1
        await trio.sleep(1)

async def test_lightdb_reported(device):
    await wait_for_lightdb(device)

    # Check if all values in desired state were replicated
    assert (await device.lightdb.get('hil/lightdb/reported/sync/true')) is True
    assert (await device.lightdb.get('hil/lightdb/reported/sync/false')) is False
    assert (await device.lightdb.get('hil/lightdb/reported/sync/int')) == 13
    assert (await device.lightdb.get('hil/lightdb/reported/sync/float')) == -234.25
    assert (await device.lightdb.get('hil/lightdb/reported/sync/str')) == '15'
    assert (await device.lightdb.get('hil/lightdb/reported/sync/obj')) == {'c': '26', 'd': -12}

    assert (await device.lightdb.get('hil/lightdb/reported/sync/invalid')) in ('GOLIOTH_ERR_NULL', 'GOLIOTH_ERR_BAD_REQUEST')
    assert (await device.lightdb.get('hil/lightdb/reported/sync/nothing')) == 'GOLIOTH_ERR_NULL'

    assert (await device.lightdb.get('hil/lightdb/reported/async/true')) is True
    assert (await device.lightdb.get('hil/lightdb/reported/async/false')) is False
    assert (await device.lightdb.get('hil/lightdb/reported/async/int')) == 13
    assert (await device.lightdb.get('hil/lightdb/reported/async/float')) == -234.25
    assert (await device.lightdb.get('hil/lightdb/reported/async/str')) == '15'
    assert (await device.lightdb.get('hil/lightdb/reported/async/obj')) == {'c': '26', 'd': -12}

async def test_lightdb_deleted(device):
    await wait_for_lightdb(device)

    assert (await device.lightdb.get('hil/lightdb/to_delete/sync/true')) is None
    assert (await device.lightdb.get('hil/lightdb/to_delete/sync/false')) is None
    assert (await device.lightdb.get('hil/lightdb/to_delete/sync/int')) is None
    assert (await device.lightdb.get('hil/lightdb/to_delete/sync/str')) is None
    assert (await device.lightdb.get('hil/lightdb/to_delete/sync/obj')) is None

    assert (await device.lightdb.get('hil/lightdb/to_delete/async/true')) is None
    assert (await device.lightdb.get('hil/lightdb/to_delete/async/false')) is None
    assert (await device.lightdb.get('hil/lightdb/to_delete/async/int')) is None
    assert (await device.lightdb.get('hil/lightdb/to_delete/async/str')) is None
    assert (await device.lightdb.get('hil/lightdb/to_delete/async/obj')) is None

async def test_lightdb_invalid(device):
    await wait_for_lightdb(device)

    assert (await device.lightdb.get('hil/lightdb/invalid/sync/set_dot')) == 'GOLIOTH_ERR_COAP_RESPONSE'

class Coap_Content_Type(Enum):
    JSON = 1
    CBOR = 2

class ObserveTester:
    def __init__(self, device):
        self.events_count = 1
        self.device = device

    async def set(self, value: int, content_type: Coap_Content_Type):
        self.events_count += 1

        if content_type == Coap_Content_Type.JSON:
            observe_path = 'hil/lightdb/observed/int'
        elif content_type == Coap_Content_Type.CBOR:
            observe_path = 'hil/lightdb/observed/cbor/int'
        else:
            assert False

        await self.device.lightdb.set(observe_path, value)
        with trio.fail_after(10):
            count = None

            while count is None or count < self.events_count:
                await trio.sleep(1)
                count = await self.device.lightdb.get('hil/lightdb/observed/events/count')

async def test_lightdb_observed(device):
    ot = ObserveTester(device)

    await ot.set(15, Coap_Content_Type.JSON)
    await ot.set(17, Coap_Content_Type.JSON)
    await ot.set(19, Coap_Content_Type.CBOR)
    await ot.set(21, Coap_Content_Type.CBOR)

    assert (await device.lightdb.get('hil/lightdb/observed/events/count')) == 5
    assert (await device.lightdb.get('hil/lightdb/observed/events/0')) == 0 # Initial JSON value
    assert (await device.lightdb.get('hil/lightdb/observed/events/1')) == 0 # Initial CBOR value
    assert (await device.lightdb.get('hil/lightdb/observed/events/2')) == 15
    assert (await device.lightdb.get('hil/lightdb/observed/events/3')) == 17
    assert (await device.lightdb.get('hil/lightdb/observed/events/4')) == 19
    assert (await device.lightdb.get('hil/lightdb/observed/events/5')) == 21
