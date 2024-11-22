import golioth
import os
import pytest

UPDATE_VERSION = "255.8.9"
UPDATE_PACKAGE = "main"


@pytest.fixture(scope="session")
def anyio_backend():
    return "trio"


@pytest.fixture(scope="session")
async def fw_info():
    return {"package": UPDATE_PACKAGE, "version": UPDATE_VERSION}


@pytest.fixture(scope="module")
async def blueprint_id(project, request):
    bp_name = request.config.option.platform[0].replace("/", "_")
    print(request.config.option.platform.replace("/", "_"))
    print(request.config.option.platform[0].replace("/", "_"))
    yield await project.blueprints.get_id(bp_name)


@pytest.fixture(scope="module")
async def tag(project, device, blueprint_id):
    tag_name = device.name.lower().replace("-", "_")
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
        if (
            a.blueprint == blueprint_id
            and a.version == UPDATE_VERSION
            and a.package == UPDATE_PACKAGE
        ):
            artifact = a

    assert artifact != None

    yield artifact


@pytest.fixture(scope="module")
async def release(project, artifact, tag):
    release = await project.releases.create([artifact.id], [], [tag.id], False)
    yield release

    # Clean Up

    await project.releases.delete(release.id)
