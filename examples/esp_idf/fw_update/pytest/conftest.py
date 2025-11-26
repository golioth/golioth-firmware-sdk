# Copyright (c) 2024 Golioth, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import pytest

UPDATE_VERSION = '255.8.9'

@pytest.fixture(scope='session')
def anyio_backend():
    return 'trio'

@pytest.fixture(scope="session")
async def target_package(request):
    return f"{request.config.getoption('board')}"

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
