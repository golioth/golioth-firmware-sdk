import logging
from pathlib import Path
import subprocess
import datetime
import logging
from time import sleep

import pytest
import west.configuration

WEST_TOPDIR = Path(west.configuration.west_dir()).parent

LOGGER = logging.getLogger(__name__)

FS_SUBDIR = "/lfs1/credentials"
FS_CRT_PATH = f"{FS_SUBDIR}/client_cert.der"
FS_KEY_PATH = f"{FS_SUBDIR}/private_key.der"

pytestmark = pytest.mark.anyio

def subprocess_logger(result, log_msg):
    if result.stdout:
        LOGGER.info(f'{log_msg} stdout: {result.stdout}')
    if result.stderr:
        LOGGER.error(f'{log_msg} stderr: {result.stderr}')

async def test_credentials(shell, project, device_name, mcumgr_conn_args, certificate_cred, wifi_ssid, wifi_psk):
    # Check cloud to verify device does not exist

    with pytest.raises(Exception):
        device = await project.device_by_name(device_name)

    # Generate device certificates

    subprocess.run([WEST_TOPDIR / "modules/lib/golioth-firmware-sdk/scripts/certificates/generate_device_certificate.sh",
                    project.info['id'], device_name])

    # Set WiFi credential

    if wifi_ssid is not None:
        shell.exec_command(f"settings set wifi/ssid \"{wifi_ssid}\"")

    if wifi_psk is not None:
        shell.exec_command(f"settings set wifi/psk \"{wifi_psk}\"")

    # Set Golioth credential

    shell._device.readlines_until(regex=".*Could not stat /lfs1/credentials/client_cert.der", timeout=180.0)

    shell.exec_command('fs mkdir /lfs1/credentials')
    shell.exec_command('log halt')

    result = subprocess.run(["mcumgr"] + mcumgr_conn_args +
                            ["--tries=3", "--timeout=2",
                             "fs", "upload",
                             f"{project.info['id']}-{device_name}.crt.der", FS_CRT_PATH],
                            capture_output = True, text = True)
    subprocess_logger(result, 'mcumgr crt')
    assert result.returncode == 0

    result = subprocess.run(["mcumgr"] + mcumgr_conn_args +
                            ["--tries=3", "--timeout=2",
                             "fs", "upload",
                             f"{project.info['id']}-{device_name}.key.der", FS_KEY_PATH],
                            capture_output = True, text = True)
    subprocess_logger(result, 'mcumgr key')
    assert result.returncode == 0

    shell.exec_command('log go')
    shell._device.clear_buffer()

    # Await connection

    shell._device.readlines_until(regex=".*Golioth client connected", timeout=90.0)
    shell._device.readlines_until(regex=".*Sending hello! 2", timeout=20.0)

    # Check cloud to verify device was created
    device = await project.device_by_name(device_name)
    assert device.name == device_name
