from contextlib import suppress
from pathlib import Path
import random
import string
import subprocess
import sys

import pytest
import west.configuration

from twister_harness.device.binary_adapter import NativeSimulatorAdapter

WEST_TOPDIR = Path(west.configuration.west_dir()).parent

sys.path.insert(0, str(WEST_TOPDIR / 'zephyr' / 'scripts' / 'west_commands'))
from runners.core import BuildConfiguration

@pytest.fixture
def mcumgr_conn_args(request, dut):
    build_conf = BuildConfiguration(request.config.option.build_dir)

    if isinstance(dut, NativeSimulatorAdapter):
        if 'CONFIG_NET_NATIVE_OFFLOADED_SOCKETS' in build_conf:
            ip = '127.0.0.1'
        else:
            ip = build_conf['CONFIG_NET_CONFIG_MY_IPV4_ADDR']

        return [
            "--conntype=udp",
            f"--connstring={ip}:1337",
        ]

    return [
        "--conntype=serial",
        f"--connstring=dev={request.config.option.device_serial},baud=115200"
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

    with suppress(Exception):
        await project.delete_device_by_name(name)
