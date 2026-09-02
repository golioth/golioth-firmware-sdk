import pytest
import sys
import west.configuration
from pathlib import Path
from twister_harness.helpers.domains_helper import get_default_domain_name

WEST_TOPDIR = Path(west.configuration.west_dir()).parent

sys.path.insert(0, str(WEST_TOPDIR / 'zephyr' / 'scripts' / 'west_commands'))
from runners.core import BuildConfiguration

UPDATE_VERSION = '255.8.9'

@pytest.fixture(scope='session')
def anyio_backend():
    return 'trio'

@pytest.fixture(scope="session")
async def target_package(request):
    build_dir = Path(request.config.option.build_dir)
    domains = build_dir / 'domains.yaml'
    assert domains.exists()
    app_build_dir = build_dir / get_default_domain_name(domains)
    build_conf = BuildConfiguration(str(app_build_dir))
    package_name = build_conf['CONFIG_GOLIOTH_FW_UPDATE_PACKAGE_NAME']
    assert package_name != ""
    return package_name

@pytest.fixture(scope="session")
async def fw_info(target_package):
    return {"package": target_package, "version": UPDATE_VERSION}

@pytest.fixture(scope="module")
async def cohort(project, device):
    cohort_name = device.name.lower().replace('-','_')
    cohort = await project.cohorts.create(cohort_name)

    await device.update_cohort(cohort.id)

    yield cohort

    try:
        await device.remove_cohort()
    except Exception as e:
        pass

    await project.cohorts.delete(cohort.id)

@pytest.fixture(scope="module")
async def artifact(project, target_package):
    # Find Artifact that matches this device and desired update version

    artifact = None
    all_artifacts = await project.artifacts.get_all()
    for a in all_artifacts:
        if (a.package == target_package and
            a.version == UPDATE_VERSION):
            artifact = a

    assert artifact is not None

    yield artifact
