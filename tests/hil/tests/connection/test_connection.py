import pytest

pytestmark = pytest.mark.anyio

async def test_connect(board, device):
    # Set Golioth credentials
    golioth_cred = (await device.credentials.list())[0]
    board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Confirm connection to Golioth
    assert None != board.wait_for_regex_in_line('Golioth CoAP client connected', timeout_s=120)

    # Wait for reconnection after golioth_client_stop();
    assert None != board.wait_for_regex_in_line('Stopping client', timeout_s=15)
    assert None != board.wait_for_regex_in_line('Starting client', timeout_s=120)
    assert None != board.wait_for_regex_in_line('Golioth CoAP client connected', timeout_s=120)

    # Wait for reconnection after golioth_client_destroy();
    assert None != board.wait_for_regex_in_line('Destroying client', timeout_s=15)
    assert None != board.wait_for_regex_in_line('Starting client', timeout_s=120)
    assert None != board.wait_for_regex_in_line('Golioth CoAP client connected', timeout_s=120)
