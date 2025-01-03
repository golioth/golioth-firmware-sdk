import logging
import pytest
from datetime import datetime, timezone
import re

pytestmark = pytest.mark.anyio

async def test_hello(board, device):
    # Set Golioth credential
    golioth_cred = (await device.credentials.list())[0]
    await board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Record timestamp and wait for fourth hello message
    start = datetime.now(timezone.utc)
    await board.wait_for_regex_in_line('.*Hello, Golioth 2!', timeout_s=90.0)

    # Check logs for hello messages
    end = datetime.now(timezone.utc)
    logs = await device.get_logs({'start': start.strftime('%Y-%m-%dT%H:%M:%S.%fZ'), 'end': end.strftime('%Y-%m-%dT%H:%M:%S.%fZ')})

    # Test logs received from server
    r = re.compile(".*Hello, Golioth 1!")
    matching_log = list(filter(r.match, [str(l) for l in logs]))
    assert len(matching_log) == 1
