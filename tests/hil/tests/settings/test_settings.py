import pytest
import time

SUCCESS = 0
KEY_NOT_RECOGNIZED = 1
KEY_NOT_VALID = 2
VALUE_FORMAT_NOT_VALID = 3
VALUE_OUTSIDE_RANGE = 4
VALUE_STRING_TOO_LONG = 5
GENERAL_ERROR = 6

SYNC_MAX_RETRY = 10

pytestmark = pytest.mark.anyio

@pytest.fixture(autouse=True, scope="module")
async def setup(project, board, device):
    # Delete any existing device-level settings

    settings = await device.settings.get_all()
    for setting in settings:
        if 'deviceId' in setting:
            await device.settings.delete(setting['key'])

    # Setting will be an unknown type depending on previous test. Delete for fresh start.

    await project.settings.delete('TEST_WRONG_TYPE')

    # Ensure the project-level settings exist

    await project.settings.set('TEST_INT', 1)
    await project.settings.set('TEST_INT_RANGE', 1)
    await project.settings.set('TEST_BOOL', False)
    await project.settings.set('TEST_FLOAT', 1.23)
    await project.settings.set('TEST_STRING', "foo")
    await project.settings.set('TEST_NOT_REGISTERED', 27)
    await project.settings.set('TEST_WRONG_TYPE', "wrong")
    await project.settings.set('TEST_CANCEL', False)

    # Set Golioth credentials
    golioth_cred = (await device.credentials.list())[0]
    board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Confirm connection to Golioth
    assert None != board.wait_for_regex_in_line('Golioth CoAP client connected', timeout_s=120)

@pytest.fixture(scope="module")
async def facilitate_in_sync(project, device):
    # Remove settings that prevent successful sync

    await project.settings.delete('TEST_NOT_REGISTERED')
    await project.settings.delete('TEST_WRONG_TYPE')

    # Delete any existing device-level settings

    settings = await device.settings.get_all()
    for setting in settings:
        if 'deviceId' in setting:
            await device.settings.delete(setting['key'])

    # Add setting value to allow successful sync

    await project.settings.set('TEST_WRONG_TYPE', 14)

    yield

    # Cleanup by resetting to expected invalid settings

    await project.settings.delete('TEST_WRONG_TYPE')
    await project.settings.set('TEST_NOT_REGISTERED', 27)
    await project.settings.set('TEST_WRONG_TYPE', "wrong")

async def assert_settings_error(device, key, error):
    await device.refresh()

    key_found = False

    for e in device.info['metadata']['lastSettingsStatus']['errors']:
        if e['key'] == key:
            key_found = True
            assert e['code'] == error

    assert key_found or error is None

async def verify_sync_status(project, device, board, is_in_sync : bool):
    if is_in_sync:
        sync_status = "in-sync"
    else:
        sync_status = "out-of-sync"

    # Make multiple attempts at receiving the desired sync status

    for i in range(SYNC_MAX_RETRY):
        try:
            await device.refresh()
            assert f"{sync_status}" == device.info["metadata"]["lastSettingsStatus"]["status"]
            return

        except:
            print(f'Settings unexpectedly {device.info["metadata"]["lastSettingsStatus"]["status"]}'
                  f'... Will retry {i}')

            # Toggle setting to give device another chance to update sync status

            next_status = i % 2 == 0
            return_val = await project.settings.set('TEST_BOOL', next_status)
            print("bool op:", return_val)

            try:
                board.wait_for_regex_in_line(f'Received test_bool: {next_status}', timeout_s=10)
            except:
                print("Failed to see TEST_BOOL update on device serial output.")

    # Final assert should fail if we haven't already returned

    assert f"{sync_status}" == device.info["metadata"]["lastSettingsStatus"]["status"]

##### Tests #####

async def test_set_int(board, device):
    await device.settings.set('TEST_INT', 42)

    assert None != board.wait_for_regex_in_line('Received test_int: 42', timeout_s=10)

    # Wait for device to respond to server
    time.sleep(1)

    await assert_settings_error(device, 'TEST_INT', None)

async def test_int_too_large(board, device):
    await device.settings.set('TEST_INT', 2**33)

    with pytest.raises(RuntimeError):
        board.wait_for_regex_in_line('Received test_int: 8589934592', timeout_s=10)

    # Wait for device to respond to server
    time.sleep(1)

    await assert_settings_error(device, 'TEST_INT', VALUE_OUTSIDE_RANGE)

async def test_int_too_small(board, device):
    await device.settings.set('TEST_INT', -1*(2**33))

    with pytest.raises(RuntimeError):
        board.wait_for_regex_in_line('Received test_int: -8589934592', timeout_s=10)

    # Wait for device to respond to server
    time.sleep(1)

    await assert_settings_error(device, 'TEST_INT', VALUE_OUTSIDE_RANGE)

async def test_set_int_range(board, device):
    await device.settings.set('TEST_INT_RANGE', 5)

    assert None != board.wait_for_regex_in_line('Received test_int_range: 5', timeout_s=10)

    # Wait for device to respond to server
    time.sleep(1)

    await assert_settings_error(device, 'TEST_INT_RANGE', None)

async def test_set_int_range_min(board, device):
    await device.settings.set('TEST_INT_RANGE', 0)

    assert None != board.wait_for_regex_in_line('Received test_int_range: 0', timeout_s=10)

    # Wait for device to respond to server
    time.sleep(1)

    await assert_settings_error(device, 'TEST_INT_RANGE', None)

async def test_set_int_range_max(board, device):
    await device.settings.set('TEST_INT_RANGE', 100)

    assert None != board.wait_for_regex_in_line('Received test_int_range: 100', timeout_s=10)

    # Wait for device to respond to server
    time.sleep(1)

    await assert_settings_error(device, 'TEST_INT_RANGE', None)

async def test_set_int_range_out_min(board, device):
    await device.settings.set('TEST_INT_RANGE', -1)

    with pytest.raises(RuntimeError):
        board.wait_for_regex_in_line('Received test_int_range: -1', timeout_s=10)

    # Wait for device to respond to server
    time.sleep(1)

    await assert_settings_error(device, 'TEST_INT_RANGE', VALUE_OUTSIDE_RANGE)

async def test_set_int_range_out_max(board, device):
    await device.settings.set('TEST_INT_RANGE', 101)

    with pytest.raises(RuntimeError):
        assert None != board.wait_for_regex_in_line('Received test_int_range: 101', timeout_s=10)

    # Wait for device to respond to server
    time.sleep(1)

    await assert_settings_error(device, 'TEST_INT_RANGE', VALUE_OUTSIDE_RANGE)

async def test_set_bool(board, device):
    await device.settings.set('TEST_BOOL', True)

    assert None != board.wait_for_regex_in_line('Received test_bool: true', timeout_s=10)

    # Wait for device to respond to server
    time.sleep(1)

    await assert_settings_error(device, 'TEST_BOOL', None)

async def test_set_float(board, device):
    await device.settings.set('TEST_FLOAT', 3.14)

    assert None != board.wait_for_regex_in_line('Received test_float: 3.14', timeout_s=10)

    # Wait for device to respond to server
    time.sleep(1)

    await assert_settings_error(device, 'TEST_FLOAT', None)

async def test_set_float_whole(board, device):
    await device.settings.set('TEST_FLOAT', 10)

    assert None != board.wait_for_regex_in_line('Received test_float: 10', timeout_s=10)

    # Wait for device to respond to server
    time.sleep(1)

    await assert_settings_error(device, 'TEST_FLOAT', None)

async def test_set_string(board, device):
    await device.settings.set('TEST_STRING', "bar")

    assert None != board.wait_for_regex_in_line('Received test_string: bar', timeout_s=10)

    # Wait for device to respond to server
    time.sleep(1)

    await assert_settings_error(device, 'TEST_STRING', None)

async def test_unrecognized_key(device):
    await assert_settings_error(device, "TEST_NOT_REGISTERED", KEY_NOT_RECOGNIZED)

async def test_wrong_type(device):
    await assert_settings_error(device, 'TEST_WRONG_TYPE', VALUE_FORMAT_NOT_VALID)

@pytest.mark.usefixtures("facilitate_in_sync")
async def test_cancel_all(project, board, device):

    # Verify in-sync
    await verify_sync_status(project, device, board, True)

    # Cancel all settings
    await device.settings.set('TEST_CANCEL', True)

    assert None != board.wait_for_regex_in_line('Cancelling settings', timeout_s=10)
    assert None != board.wait_for_regex_in_line('Settings have been cancelled', timeout_s=10)

    await device.settings.set('TEST_CANCEL', False)

    # Wait for device to respond to server
    time.sleep(1)

    # Verify out-of-sync
    # await verify_sync_status(project, device, board, False)

    # Wait for device to automatically re-register all settings
    assert None != board.wait_for_regex_in_line('Settings have been reregistered', timeout_s=120)

    # Verify in-sync
    await verify_sync_status(project, device, board, True)
