import logging
import pytest
import pprint
import time
import yaml
import datetime
import logging

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_hello(shell, device, credentials_file):
    # Read credentials

    with open(credentials_file, 'r') as f:
        credentials = yaml.safe_load(f)

    time.sleep(2)

    # Set credentials

    for setting in credentials['settings']:
        shell.exec_command(f"settings set {setting} \"{credentials['settings'][setting]}\"")

    shell._device.clear_buffer()
    shell._device.write('kernel reboot cold\n\n'.encode())

    # Record timestamp and wait for fourth hello mesage

    start = datetime.datetime.utcnow()
    shell._device.readlines_until(regex=".*Sending hello! 3", timeout=90.0)

    # Check logs for hello messages

    end = datetime.datetime.utcnow()

    logs = await device.get_logs({'start': start.strftime('%Y-%m-%dT%H:%M:%S.%fZ'), 'end': end.strftime('%Y-%m-%dT%H:%M:%S.%fZ')})

    # Test logs received from server

    LOGGER.info("Searching log messages from end to start:")
    test_idx = 2
    test_hits = 0
    for m in reversed(logs):

        if m.message == f"Sending hello! {test_idx}":
            LOGGER.info("### MATCH FOUND! ---> {0}".format(m.message))
            test_hits += 1
            test_idx -= 1
            if test_idx < 0:
                break
        else:
            LOGGER.info(m.message)

    assert test_hits == 3, 'Unable to find all Hello messages on server'
