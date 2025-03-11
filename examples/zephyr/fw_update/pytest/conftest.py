import logging
from pathlib import Path

import pytest
from twister_harness.device.device_adapter import DeviceAdapter

UPDATE_VERSION = '255.8.9'
UPDATE_PACKAGE = 'main'

logger = logging.getLogger()


@pytest.fixture(scope='session')
def anyio_backend():
    return 'trio'


@pytest.fixture(scope="session")
async def fw_info():
    return {"package": UPDATE_PACKAGE, "version": UPDATE_VERSION}


async def blueprint_name(project, request):
    return request.config.option.platform.replace('/','_')


@pytest.fixture(scope="module")
async def blueprint_id(project, request):
    bp_name = request.config.option.platform.replace('/','_')
    yield await project.blueprints.get_id(bp_name)


@pytest.fixture(scope='module', autouse=True)
async def upload_next(project, blueprint_id, request):
    # TODO: detect whether SB_CONFIG_FW_UPDATE_NEXT=y
    # artifacts_ids = []

    # releases = await project.releases.get_all()
    # for release in releases:
    #     if '255.8.9' in release.release_tags:
    #         artifacts_ids += release.artifact_ids
    #         await project.releases.delete(release.id)

    # blueprints = await project.blueprints.get_all()
    # for blueprint in blueprints:
    #     if blueprint.name == blueprint_name:
    #         break
    # else:
    #     await project.blueprints.create(blueprint_name, 'Zephyr', 'ESP-32')

    logger.info(f'Blueprint id: {blueprint_id}')

    logger.info('Deleting old artifact')

    artifacts = await project.artifacts.get_all()
    for artifact in artifacts:
        if (artifact.blueprint == blueprint_id and
            artifact.version == UPDATE_VERSION and
            artifact.package == UPDATE_PACKAGE):
            await project.artifacts.delete(artifact.id)

    logger.info('Uploading new artifact')

    await project.artifacts.upload(Path(request.config.option.build_dir) / 'fw_update_next' / 'zephyr' / 'zephyr.signed.bin',
                                   version=UPDATE_VERSION,
                                   package=UPDATE_PACKAGE,
                                   blueprint_id=blueprint_id)


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
