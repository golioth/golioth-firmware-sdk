import datetime
import logging
import pytest
import trio

LOGGER = logging.getLogger(__name__)

LOG_FETCH_COUNT_LIMIT = 5

UPDATE_PACKAGE = 'main'
DUMMY_VER_OLDER = '1.2.2'
DUMMY_VER_SAME = '1.2.3'
DUMMY_VER_UPDATE = '1.2.4'
DUMMY_VER_EXTRA = '1.2.5'
MULTI_PACKAGE = 'walrus'
MULTI_PACKAGE_VER = '0.4.2'

golioth_ota_state = [
    "IDLE",
    "DOWNLOADING",
    "DOWNLOADED",
    "UPDATING"
    ]

golioth_ota_reason = [
    "GOLIOTH_OTA_REASON_READY",
    "GOLIOTH_OTA_REASON_FIRMWARE_UPDATED_SUCCESSFULLY",
    "GOLIOTH_OTA_REASON_NOT_ENOUGH_FLASH_MEMORY",
    "GOLIOTH_OTA_REASON_OUT_OF_RAM",
    "GOLIOTH_OTA_REASON_CONNECTION_LOST",
    "GOLIOTH_OTA_REASON_INTEGRITY_CHECK_FAILURE",
    "GOLIOTH_OTA_REASON_UNSUPPORTED_PACKAGE_TYPE",
    "GOLIOTH_OTA_REASON_INVALID_URI",
    "GOLIOTH_OTA_REASON_FIRMWARE_UPDATE_FAILED",
    "GOLIOTH_OTA_REASON_UNSUPPORTED_PROTOCOL"
    ]

GOLIOTH_OTA_STATE_DOWNLOADING = 1
GOLIOTH_OTA_REASON_READY = 0
GOLIOTH_OTA_STATE_CNT = len(golioth_ota_state)
GOLIOTH_OTA_REASON_CNT = len(golioth_ota_reason)
TEST_BLOCK_CNT = 42

pytestmark = pytest.mark.anyio

@pytest.fixture(autouse=True, scope="module")
async def setup(board, device):
    # Set Golioth credentials
    golioth_cred = (await device.credentials.list())[0]
    await board.set_golioth_psk_credentials(golioth_cred.identity, golioth_cred.key)

    # Confirm connection to Golioth
    assert None is not await board.wait_for_regex_in_line('Golioth CoAP client connected', timeout_s=120)

@pytest.fixture(scope="module")
async def tag(project, device):
    tag_name = device.name.lower().replace('-','_')
    tag = await project.tags.create(tag_name)

    await device.add_tag(tag.id)

    yield tag

    try:
        await device.remove_tag(tag.id)
    except Exception:
        pass

    await project.tags.delete(tag.id)

@pytest.fixture(scope="module")
async def artifacts(project):
    artifacts = dict()

    # Find Artifact that matches this device and desired update versions

    all_artifacts = await project.artifacts.get_all()
    for a in all_artifacts:
        if a.blueprint is None:
            if a.version == DUMMY_VER_OLDER and a.package == UPDATE_PACKAGE:
                artifacts["test_manifest"] = a
            elif a.version == DUMMY_VER_SAME and a.package == UPDATE_PACKAGE:
                artifacts["test_blocks"] = a
            elif a.version == DUMMY_VER_UPDATE and a.package == UPDATE_PACKAGE:
                artifacts["test_reasons"] = a
            elif a.version == DUMMY_VER_EXTRA and a.package == UPDATE_PACKAGE:
                artifacts["test_multiple"] = a
            elif a.version == MULTI_PACKAGE_VER and a.package == MULTI_PACKAGE:
                artifacts["multi_artifact"] = a

    assert len(artifacts) == 5

    return artifacts

@pytest.fixture(scope="module")
async def cohort(project, device):
    cohort_name = device.name.lower().replace('-','_')
    cohort = await project.cohorts.create(cohort_name)

    await device.update_cohort(cohort.id)

    yield cohort

    try:
        await device.remove_cohort()
    except Exception:
        pass

    await project.cohorts.delete(cohort.id)

async def verify_component_values(board, info):
    package_info = info["package"]
    version_info = info["version"]
    b_info = info["binaryInfo"]

    assert None is not await board.wait_for_regex_in_line(f'component.package: {package_info}', timeout_s=2)
    assert None is not await board.wait_for_regex_in_line(f'component.version: {version_info}', timeout_s=2)
    assert None is not await board.wait_for_regex_in_line(f'component.size: {b_info["size"]}', timeout_s=2)
    assert None is not await board.wait_for_regex_in_line(f'component.hash: {b_info["digests"]["sha256"]["digest"]}', timeout_s=2)

def verify_dfu_status_logs(logs):
    assert len(logs) == len(golioth_ota_reason)

    for i, r in enumerate(golioth_ota_reason):
        assert int(logs[i].metadata['reasonCode']) == i
        assert int(logs[i].metadata['stateCode']) == i % GOLIOTH_OTA_STATE_CNT
        assert logs[i].metadata['state'] == golioth_ota_state[ i % GOLIOTH_OTA_STATE_CNT ]
        assert logs[i].metadata['package'] == "lobster"
        assert logs[i].metadata['version'] == "2.3.4"
        assert logs[i].metadata['target'] == "5.6.7"

##### Tests #####

async def test_manifest(artifacts, board, project, cohort):
    await cohort.deployments.create("test_manifest", [artifacts["test_manifest"].id])

    assert None is not await board.wait_for_regex_in_line('Manifest received', timeout_s=30)

    assert None is not await board.wait_for_regex_in_line('Manifest successfully decoded', timeout_s=2)

    # We may need to wait for the second manifest to trigger this line (the first has no rollouts)

    assert None is not await board.wait_for_regex_in_line('Found main component', timeout_s=30)
    assert None is not await board.wait_for_regex_in_line('No absent component found', timeout_s=2)

    # Test all parts of OTA component

    await verify_component_values(board, artifacts["test_manifest"].info)

async def test_multiple_artifacts(artifacts, board, project, cohort):
    await cohort.deployments.create("test_multiple", [artifacts["test_multiple"].id,
                                                      artifacts["multi_artifact"].id])

    assert None is not await board.wait_for_regex_in_line('Manifest received', timeout_s=30)
    assert None is not await board.wait_for_regex_in_line('Manifest successfully decoded', timeout_s=2)
    assert None is not await board.wait_for_regex_in_line('Found main component', timeout_s=30)
    assert None is not await board.wait_for_regex_in_line('Found walrus component', timeout_s=2)

    # Test all parts of OTA component

    await verify_component_values(board, artifacts["multi_artifact"].info)

    # Test manifest get
    assert None is not await board.wait_for_regex_in_line('Manifest get SHA matches stored SHA', timeout_s=30)

    # Test manifest blockwise
    assert None is not await board.wait_for_regex_in_line('Manifest blockwise SHA matches stored SHA', timeout_s=30)

async def test_block_operations(board, project, artifacts, cohort):
    await cohort.deployments.create("test_blocks", [artifacts["test_blocks"].id])
    assert None is not await board.wait_for_regex_in_line(f"golioth_ota_size_to_nblocks: {TEST_BLOCK_CNT + 1}", timeout_s=12)

    # Test NULL client

    assert None is not await board.wait_for_regex_in_line("Block sync failed: 5", timeout_s=12)

    # Test block download

    assert None is not await board.wait_for_regex_in_line("Received block 0", timeout_s=4)
    assert None is not await board.wait_for_regex_in_line("is_last: 0", timeout_s=4)
    assert None is not await board.wait_for_regex_in_line("Received block 1", timeout_s=4)
    assert None is not await board.wait_for_regex_in_line("is_last: 1", timeout_s=40)

async def test_resume(board, project, artifacts, cohort):
    await cohort.deployments.create("test_resume", [artifacts["multi_artifact"].id])

    assert None is not await board.wait_for_regex_in_line('Manifest received', timeout_s=30)
    assert None is not  await board.wait_for_regex_in_line('Manifest successfully decoded', timeout_s=2)
    assert None is not  await board.wait_for_regex_in_line('Start resumable blockwise download test', timeout_s=10)
    assert None is not  await board.wait_for_regex_in_line('Block download failed', timeout_s=10)
    assert None is not  await board.wait_for_regex_in_line('SHA256 matches as expected', timeout_s=20)
    assert None is not  await board.wait_for_regex_in_line('Block download successful', timeout_s=2)

async def test_reason_and_state(board, device, project, artifacts, cohort):
    # Test reason and state code updates

    # Record timestamp and use rollout to trigger the test

    start = datetime.datetime.now(datetime.UTC)
    await cohort.deployments.create("test_reasons", [artifacts["test_reasons"].id])

    # After publishing all status/response updates, device will log its own last state

    await board.wait_for_regex_in_line(f"golioth_ota_get_state: {(GOLIOTH_OTA_REASON_CNT - 1) %
                                                                 GOLIOTH_OTA_STATE_CNT}",
                                       timeout_s=(GOLIOTH_OTA_REASON_CNT * 2))

    # The last status test will be an error condition, monitor serial for this event

    await board.wait_for_regex_in_line("GOLIOTH_ERR_NULL: 5", timeout_s=6)

    # Pause to ensure logs propagate to cloud

    await trio.sleep(5)

    # Check logs for firmware status updates

    log_fetch_count = 0
    while 1:
        end = datetime.datetime.now(datetime.UTC)

        logs = await device.get_logs(
            {
                "start": start.strftime("%Y-%m-%dT%H:%M:%S.%fZ"),
                "end": end.strftime("%Y-%m-%dT%H:%M:%S.%fZ"),
                "module": "golioth_dfu",
            }
        )

        if len(logs) == GOLIOTH_OTA_REASON_CNT:
            break

        assert log_fetch_count < LOG_FETCH_COUNT_LIMIT, (
            f"After {log_fetch_count} retries, got {len(logs)} dfu updates but expected {GOLIOTH_OTA_REASON_CNT}"
        )

        LOGGER.info(
            f"Fetched {len(logs)} dfu updates but expected {GOLIOTH_OTA_REASON_CNT}... will retry"
        )

        log_fetch_count += 1
        await trio.sleep(5)

    # Test logs received from server

    verify_dfu_status_logs(logs)
