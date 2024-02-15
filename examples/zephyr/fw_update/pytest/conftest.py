import golioth
import os
import pytest

UPDATE_VERSION = '255.8.9'
UPDATE_PACKAGE = 'main'


def pytest_addoption(parser):
    parser.addoption("--wifi-ssid", type=str, help="WiFi SSID")
    parser.addoption("--wifi-psk",  type=str, help="WiFi PSK")
    parser.addoption("--west-board", type=str,
                     help="Name of the board as specified in the Zephyr tree")


@pytest.fixture(scope='session')
def anyio_backend():
    return 'trio'

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

@pytest.fixture(scope="session")
def west_board(request):
    if request.config.getoption("--west-board") is not None:
        return request.config.getoption("--west-board")
    else:
        return os.environ['west_board']


@pytest.fixture(scope="session")
async def fw_info():
    return {"package": UPDATE_PACKAGE, "version": UPDATE_VERSION}


@pytest.fixture(scope="module")
async def blueprint_id(project, west_board):
    yield await project.blueprints.get_id(west_board)


@pytest.fixture(scope="module")
async def tag(project, device, blueprint_id):
    tag_name = device.name.lower().replace('-','_')
    tag = await project.tags.create(tag_name)

    await device.add_blueprint(blueprint_id)
    await device.add_tag(tag.id)

    yield tag

    try:
        await device.remove_tag(tag.id)
    except:
        pass

    await project.tags.delete(tag.id)


@pytest.fixture(scope="module")
async def artifact(project, blueprint_id):
    # Find Artifact that matches this device and desired update version

    artifact = None
    all_artifacts = await project.artifacts.get_all()
    for a in all_artifacts:
        if (a.blueprint == blueprint_id and
            a.version == UPDATE_VERSION and
            a.package == UPDATE_PACKAGE):
            artifact = a

    assert artifact != None

    yield artifact


@pytest.fixture(scope="module")
async def release(project, artifact, tag):
    release = await project.releases.create([artifact.id], [], [tag.id], False)
    yield release

    # Clean Up

    await project.releases.delete(release.id)
