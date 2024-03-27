import os
import pytest
import random
import string
import subprocess

def pytest_addoption(parser):
    parser.addoption("--device-port", type=str,
                     help="Device serial port path")
    parser.addoption("--wifi-ssid", type=str, help="WiFi SSID")
    parser.addoption("--wifi-psk",  type=str, help="WiFi PSK")

@pytest.fixture(scope="session")
def device_port(request):
    if request.config.getoption("--device-port") is not None:
        return request.config.getoption("---device-port")
    else:
        port_key = f"CI_{os.environ['hil_board'].upper()}_PORT"
        return os.environ[port_key]

@pytest.fixture(scope='session')
def anyio_backend():
    return 'trio'

@pytest.fixture(scope="module")
async def certificate_cred(project):
    subprocess.run(["modules/lib/golioth-firmware-sdk/scripts/certificates/generate_root_certificate.sh"])

    # Pass root public key to Golioth server

    with open('golioth.crt.pem', 'rb') as f:
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

@pytest.fixture(scope="session")
def wifi_ssid(request):
    if request.config.getoption("--wifi-ssid") is not None:
        return request.config.getoption("--wifi-ssid")
    else:
        return os.environ['WIFI_SSID']

@pytest.fixture(scope="session")
def wifi_psk(request):
    if request.config.getoption("--wifi-psk") is not None:
        return request.config.getoption("--wifi-psk")
    else:
        return os.environ['WIFI_PSK']
