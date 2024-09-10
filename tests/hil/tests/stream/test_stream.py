import pytest
import trio
import json
import hashlib
import os

EXPECTED_HASH='28540341b9655161a00b3a7193a8e2e2261d21ea3e3f2117fc9c7f18a5733681'
BLOCK_UPLOAD_PATH='block_upload'

pytestmark = pytest.mark.anyio

@pytest.fixture(autouse=True, scope="module")
async def setup(board, device):
    # Set Golioth credentials
    golioth_cred = (await device.credentials.list())[0]
    await board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Confirm connection to Golioth
    assert None != await board.wait_for_regex_in_line('Golioth CoAP client connected', timeout_s=120)

def hash_from_dict(dict_to_hash: dict) -> str:
    str_data = json.dumps(dict_to_hash, sort_keys=True)

    h = hashlib.new('sha256')
    h.update(str_data.encode())

    return h.hexdigest()

def get_test_data_hash() -> str:
    dirname = os.path.dirname(__file__)
    filename = os.path.join(dirname, 'test_data.json')

    with open(filename, 'r') as f:
        test_data = json.load(f)

    return hash_from_dict(test_data)

##### Tests #####

async def test_block_upload(board, device):
    assert None != await board.wait_for_regex_in_line('Block upload successful', timeout_s=20)

    # Wait for stream to propagate from CDG to LightDB Stream
    await trio.sleep(6)

    # Get Stream entry of the block upload from cloud
    stream = await device.stream.get(path=BLOCK_UPLOAD_PATH, interval="1m")
    print(stream)
    assert 'list' in stream and len(stream.keys()) > 0

    blockwise_json = stream['list'][0]['data'][BLOCK_UPLOAD_PATH]

    # Get hash of the JSON string we received
    blockwise_hash = hash_from_dict(blockwise_json)
    print("Golioth data hash:", blockwise_hash)

    # Generate hash from the source JSON file
    test_hash = get_test_data_hash()
    print("Test data hash:", test_hash)

    # Assert that a hash of the block upload matches our expected hash
    assert blockwise_hash == test_hash
