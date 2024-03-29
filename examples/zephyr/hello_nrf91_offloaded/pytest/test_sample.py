import datetime
import logging
import pytest
import trio

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_hello(shell, device, build_conf):
    await trio.sleep(2)

    # Set Golioth credential

    golioth_cred = (await device.credentials.list())[0]
    credentials_tag = build_conf['CONFIG_GOLIOTH_COAP_CLIENT_CREDENTIALS_TAG']

    # Deactivate modem to allow credentials modification
    shell.exec_command(f"nrf_modem_at AT+CFUN=4", timeout=10.0)

    # List all credentials
    shell.exec_command(f"nrf_modem_at AT%CMNG=1,{credentials_tag}")

    # PSK-ID
    shell.exec_command(f"nrf_modem_at AT%CMNG=3,{credentials_tag},4")  # Delete
    shell.exec_command(f"nrf_modem_at AT%CMNG=0,{credentials_tag},4,{golioth_cred.identity}")

    # PSK
    shell.exec_command(f"nrf_modem_at AT%CMNG=3,{credentials_tag},3")  # Delete
    shell.exec_command(f"nrf_modem_at AT%CMNG=0,{credentials_tag},3,{golioth_cred.key.encode('ascii').hex()}")

    # Reboot to take effect
    shell._device.clear_buffer()
    shell._device.write('kernel reboot cold\n\n'.encode())

    # Record timestamp and wait for fourth hello message

    start = datetime.datetime.utcnow()
    shell._device.readlines_until(regex=".*Sending hello! 3", timeout=110.0)

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
