# Copyright (c) 2024 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import os
import pytest
from pathlib import Path

def pytest_addoption(parser):
    parser.addoption("--device-cert-name", type=str,
                     help="Device name to use when creating device certificate.")
    parser.addoption("--root-certificate", type=str,
                     help="Path to root certificate used to sign device certificate.")

@pytest.fixture(scope='session')
def anyio_backend():
    return 'trio'

@pytest.fixture(scope="session")
def device_cert_name(request):
    if request.config.getoption("--device-cert-name") is not None:
        return request.config.getoption("--device-cert-name")
    elif 'GOLIOTH_DEVICE_CERT_NAME' in os.environ:
        return os.environ['GOLIOTH_DEVICE_CERT_NAME']
    else:
        return None

@pytest.fixture(scope="session")
def root_certificate(request):
    if request.config.getoption("--root-certificate") is not None:
        return request.config.getoption("--root-certificate")
    elif 'GOLIOTH_ROOT_CERTIFICATE' in os.environ:
        return os.environ['GOLIOTH_ROOT_CERTIFICATE']
    else:
        return None

@pytest.fixture(scope="module")
async def certificate_cred(project, root_certificate, device_cert_name):
    # Check cloud to verify device does not exist
    with pytest.raises(Exception):
        await project.device_by_name(device_cert_name)

    # Pass root certificate to Golioth server
    cert_path = Path(root_certificate)
    assert(cert_path.is_file())

    with open(root_certificate, 'rb') as f:
        cert_pem = f.read()
    root_cert = await project.certificates.add(cert_pem, 'root')
    yield root_cert['data']['id']

    await project.certificates.delete(root_cert['data']['id'])

    try:
        await project.delete_device_by_name(device_cert_name)
    except:
        pass
