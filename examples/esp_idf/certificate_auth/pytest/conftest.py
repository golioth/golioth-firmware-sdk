# Copyright (c) 2024 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import pytest

def pytest_addoption(parser):
    parser.addoption("--certificate-name", type=str, action="store",
                     default="default_certificate_name",
                     help="Device name used when generating certificate.")
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
