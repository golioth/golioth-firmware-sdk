import logging
from pathlib import Path
import subprocess

import pytest
from twister_harness.device.device_adapter import DeviceAdapter
from twister_harness.device.hardware_adapter import HardwareAdapter
from twister_harness.device.utils import log_command
from twister_harness.helpers.domains_helper import get_default_domain_name
import west.configuration

WEST_TOPDIR = Path(west.configuration.west_dir()).parent

LOGGER = logging.getLogger(__name__)

FS_SUBDIR = "/lfs1/credentials"

pytestmark = pytest.mark.anyio

def subprocess_logger(result, log_msg):
    if result.stdout:
        LOGGER.info(f'{log_msg} stdout: {result.stdout}')
    if result.stderr:
        LOGGER.error(f'{log_msg} stderr: {result.stderr}')

@pytest.fixture
def lfs_flash_empty(device_object: DeviceAdapter, request):
    if not isinstance(device_object, HardwareAdapter):
        return

    build_dir = Path(request.config.option.build_dir)
    domains = build_dir / 'domains.yaml'
    if domains.exists():
        build_dir = build_dir / get_default_domain_name(domains)

    LOGGER.info("Flashing LFS image")
    device_object.generate_command()
    command = device_object.command + ["--hex-file", str(build_dir / "lfs.hex")]
    log_command(LOGGER, "Flashing LFS command", command, level=logging.DEBUG)
    result = subprocess.run(command,
                            capture_output=True, text=True,
                            cwd=str(build_dir),
                            check=False)
    subprocess_logger(result, 'LFS flash')
    assert result.returncode == 0

async def test_cert_provisioning(lfs_flash_empty, request, shell,
                                 project, device_name,
                                 mcumgr_conn_args, certificate_cred,
                                 wifi_ssid, wifi_psk):
    # Check cloud to verify device does not exist

    with pytest.raises(Exception):
        device = await project.device_by_name(device_name)

    # Generate device certificates

    subprocess.run([WEST_TOPDIR / "modules/lib/golioth-firmware-sdk/scripts/certificates/generate_device_certificate.sh",
                    project.info['id'], device_name],
                   cwd=request.config.option.build_dir)

    # Set WiFi credential

    if wifi_ssid is not None and wifi_psk is not None:
        shell.exec_command(f"wifi cred add -k 1 -s \"{wifi_ssid}\" -p \"{wifi_psk}\"")
        shell.exec_command("wifi cred auto_connect")

    # Set Golioth credential

    shell._device.readlines_until(regex=".*Could not stat /lfs1/credentials/crt.der", timeout=180.0)

    shell.exec_command('fs mkdir /lfs1/credentials')
    shell.exec_command('log halt')

    for component in ["crt", "key"]:
        result = subprocess.run(["mcumgr"] + mcumgr_conn_args +
                                ["--tries=3", "--timeout=2",
                                 "fs", "upload",
                                 f"{project.info['id']}-{device_name}.{component}.der", f"{FS_SUBDIR}/{component}.der"],
                                capture_output=True, text=True,
                                cwd=request.config.option.build_dir)
        subprocess_logger(result, f'mcumgr {component}')
        assert result.returncode == 0

    shell.exec_command('log go')
    shell._device.clear_buffer()

    # Await connection

    shell._device.readlines_until(regex=".*Golioth client connected", timeout=90.0)
    shell._device.readlines_until(regex=".*Sending hello! 2", timeout=20.0)

    # Check cloud to verify device was created
    device = await project.device_by_name(device_name)
    assert device.name == device_name
