from golioth import LogLevel, LogEntry
import logging
import pytest
import time
import yaml
import datetime

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

def verify_log_messages(logs):

    expected_logs = [
                     LogEntry( { 'level': LogLevel.DBG, 'message': "main: Debug info! 0"}),
                     LogEntry( { 'level': LogLevel.DBG, 'message': "func_1: Log 1: 0"}),
                     LogEntry( { 'level': LogLevel.DBG, 'message': "func_2: Log 2: 0"}),
                     LogEntry( { 'level': LogLevel.WRN, 'message': "Warn: 0"}),
                     LogEntry( { 'level': LogLevel.ERR, 'message': "Err: 0"}),
                     # A current known issue is that hexdump doesn't include second line on cloud
                     # logs. We still test for this message and verify INF type is working.
                     LogEntry( { 'level': LogLevel.INF, 'message': "Counter hexdump"})
                     ]

    LOGGER.info("Searching log messages:")

    for m in logs:
        message = m.message

        for i, e in enumerate(expected_logs):
            if m.message == e.message and m.level == e.level:
                message = "### MATCH FOUND! ---> {0}".format(m.message)
                expected_logs.pop(i)
                break

        LOGGER.info(message)

        if len(expected_logs) == 0:
            break

    num_missing = len(expected_logs)
    if num_missing > 0:
        LOGGER.error("XXX Unable to find {0} Log messages:".format(num_missing))
        for m in expected_logs:
            LOGGER.error("XXX Not FOUND ---> {0}".format(m.message))
    else:
        LOGGER.info("### All expected Log messages found!")

    assert len(expected_logs) == 0, 'Unable to find all Log messages on server'


async def test_logging(shell, device, wifi_ssid, wifi_psk):
    time.sleep(2)

    # Set Golioth credential

    golioth_cred = (await device.credentials.list())[0]
    shell.exec_command(f"settings set golioth/psk-id {golioth_cred.identity}")
    shell.exec_command(f"settings set golioth/psk {golioth_cred.key}")

    # Set WiFi credential

    shell.exec_command(f"settings set wifi/ssid \"{wifi_ssid}\"")
    shell.exec_command(f"settings set wifi/psk \"{wifi_psk}\"")

    shell._device.clear_buffer()
    shell._device.write('kernel reboot cold\n\n'.encode())

    # Record timestamp and wait for fourth hello message

    start = datetime.datetime.utcnow()
    shell._device.readlines_until(regex=".*Debug info! 2", timeout=90.0)

    # Check logs for hello messages

    end = datetime.datetime.utcnow()

    logs = await device.get_logs({'start': start.strftime('%Y-%m-%dT%H:%M:%S.%fZ'), 'end': end.strftime('%Y-%m-%dT%H:%M:%S.%fZ')})

    # Test logs received from server

    verify_log_messages(logs)
