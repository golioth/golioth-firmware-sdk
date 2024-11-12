import os
from pathlib import Path
import random
import string
import subprocess

import pytest
import west.configuration

from twister_harness.device.binary_adapter import NativeSimulatorAdapter

WEST_TOPDIR = Path(west.configuration.west_dir()).parent

def pytest_addoption(parser):
    parser.addoption("--device-port", type=str,
                     help="Device serial port path")

def get_device_port(request):
    if request.config.getoption("--device-port") is not None:
        return request.config.getoption("--device-port")
    else:
        port_key = f"CI_{os.environ['hil_board'].upper()}_PORT"
        return os.environ[port_key]

@pytest.fixture
def mcumgr_conn_args(request, dut):
    if isinstance(dut, NativeSimulatorAdapter):
        return [
            "--conntype=udp",
            "--connstring=127.0.0.1:1337",
        ]

    return [
        "--conntype=serial",
        f"--connstring=dev={get_device_port(request)},baud=115200"
    ]

@pytest.fixture(scope='session')
def anyio_backend():
    return 'trio'

@pytest.fixture(scope="module")
async def certificate_cred(request, project):
    subprocess.run([WEST_TOPDIR / "modules/lib/golioth-firmware-sdk/scripts/certificates/generate_root_certificate.sh"],
                   cwd=request.config.option.build_dir)

    # Pass root public key to Golioth server

    with open(Path(request.config.option.build_dir) / 'golioth.crt.pem', 'rb') as f:
        cert_pem = f.read()
    root_cert = await project.certificates.add(cert_pem, 'root')
    yield root_cert['data']['id']

    await project.certificates.delete(root_cert['data']['id'])

@pytest.fixture(scope="module")
async def device_name(project):
    name = 'certificate-' + ''.join(random.choice(string.ascii_uppercase + string.ascii_lowercase) for i in range(16))
    yield name

    try:
        await project.delete_device_by_name(name)
    except:
        pass
