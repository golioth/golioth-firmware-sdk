import logging
import pytest
import subprocess
import datetime
import logging

LOGGER = logging.getLogger(__name__)
PROJ_NAME = 'firmware_ci'

FS_SUBDIR = "/lfs1/credentials"
FS_CRT_PATH = f"{FS_SUBDIR}/client_cert.der"
FS_KEY_PATH = f"{FS_SUBDIR}/private_key.der"

pytestmark = pytest.mark.anyio

def subprocess_logger(result, log_msg):
    if result.stdout:
        LOGGER.info(f'{log_msg} stdout: {result.stdout}')
    if result.stderr:
        LOGGER.error(f'{log_msg} stderr: {result.stderr}')

async def test_credentials(shell, project, device_name, device_port, certificate_cred):

    # Ensure there are no credentials currently stored on device

    shell._device.readlines_until(regex=".*Start certificate provisioning sample", timeout=90.0)
    shell.exec_command(f'fs rm {FS_CRT_PATH}')
    shell.exec_command(f'fs rm {FS_KEY_PATH}')
    shell.exec_command(f'fs rm {FS_SUBDIR}')
    shell._device.clear_buffer()
    shell._device.write('kernel reboot cold\n\n'.encode())
    shell._device.readlines_until(regex=".*Start certificate provisioning sample", timeout=90.0)

    # Check cloud to verify device does not exist

    with pytest.raises(Exception):
        device = await project.device_by_name(device_name)

    # Generate device certificates

    subprocess.run(["modules/lib/golioth-firmware-sdk/scripts/certificates/generate_device_certificate.sh",
                    PROJ_NAME, device_name])

    # Set Golioth credential

    shell.exec_command('fs mkdir /lfs1/credentials')
    shell.exec_command('log halt')

    result = subprocess.run(["mcumgr", "--conntype", "serial",
                             f"--connstring=dev={device_port},baud=115200", "fs", "upload",
                             f"{PROJ_NAME}-{device_name}.crt.der", FS_CRT_PATH],
                             capture_output = True, text = True)
    subprocess_logger(result, 'mcumgr crt')
    assert result.returncode == 0

    result = subprocess.run(["mcumgr", "--conntype", "serial",
                             f"--connstring=dev={device_port},baud=115200", "fs", "upload",
                             f"{PROJ_NAME}-{device_name}.key.der", FS_KEY_PATH],
                             capture_output = True, text = True)
    subprocess_logger(result, 'mcumgr key')
    assert result.returncode == 0

    shell.exec_command('log go')
    shell._device.clear_buffer()
    shell._device.write('kernel reboot cold\n\n'.encode())

    # Await connection

    shell._device.readlines_until(regex=".*Golioth client connected", timeout=90.0)

    # Check cloud to verify device was created
    device = await project.device_by_name(device_name)
    assert device.name == device_name
