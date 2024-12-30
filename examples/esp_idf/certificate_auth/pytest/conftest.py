# Copyright (c) 2024 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pathlib import Path

def pytest_addoption(parser):
    parser.addoption("--root-certificate", type=str,
                     default="default_root_certificate",
                     help="Path to root certificate used to sign device certificate.")

@pytest.fixture(scope='session')
def anyio_backend():
    return 'trio'

@pytest.fixture(scope="session")
def certificate_name(request):
    return request.config.getoption("--certificate-name")

@pytest.fixture(scope="session")
def root_certificate(request):
    return request.config.getoption("--root-certificate")

@pytest.fixture(scope="module")
async def certificate_cred(project, root_certificate, device_name):
    # Check cloud to verify device does not exist
    with pytest.raises(Exception):
        await project.device_by_name(device_name)

    # Pass root certificate to Golioth server
    cert_path = Path(root_certificate)
    assert(cert_path.is_file())

    with open(root_certificate, 'rb') as f:
        cert_pem = f.read()
    root_cert = await project.certificates.add(cert_pem, 'root')
    yield root_cert['data']['id']

    await project.certificates.delete(root_cert['data']['id'])

    try:
        await project.delete_device_by_name(device_name)
    except:
        pass
