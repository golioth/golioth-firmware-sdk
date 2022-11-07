import sys
import os
import serial
from time import time, sleep
import re
import yaml
import requests
import random

def wait_for_str_in_line(ser, str, timeout_s=20, log=True):
    start_time = time()
    while True:
        line = ser.readline().decode('utf-8', errors='replace').replace("\r\n", "")
        if line != "" and log:
            print(line)
        if "CPU halted" in line and not "golioth_coap_client" in line:
            raise RuntimeError(line)
        if time() - start_time > timeout_s:
            raise RuntimeError('Timeout')
        if str in line:
            return line

def set_settings(ser, settings):
    print('===== Setting credentials via CLI (logging disabled) ====')
    ser.write('\r\n'.encode())
    wait_for_str_in_line(ser, 'esp32>', log=False)

    for key, value in settings.items():
        ser.write('settings set {} {}\r\n'.format(key, value).encode())
        wait_for_str_in_line(ser, 'saved', log=False)
        wait_for_str_in_line(ser, 'esp32>', log=False)

def reset(ser):
    ser.write('\r\n'.encode())
    wait_for_str_in_line(ser, 'esp32>')
    ser.write('reset\r\n'.encode())
    # Wait for string that prints on next boot
    wait_for_str_in_line(ser, 'heap_init')
    wait_for_str_in_line(ser, 'esp32>')

def green_print(s):
    green = '\033[92m'
    resetcolor = '\033[0m'
    print(green + s + resetcolor)

def red_print(s):
    red = '\033[31m'
    resetcolor = '\033[0m'
    print(red + s + resetcolor)

def run_built_in_tests(ser):
    unity_test_end_re = '\.c:\d+:test_.*:(PASS|FAIL)'
    unity_done_re = '^\d+ Tests (\d+) Failures \d+ Ignored'
    num_test_failures = 0
    test_results = []

    ser.write('\r\n'.encode())
    wait_for_str_in_line(ser, 'esp32>')
    ser.write('built_in_test\r\n'.encode())

    start_time = time()
    timeout_s = 120
    while True:
        line = ser.readline().decode('utf-8', errors='replace').replace("\r\n", "")
        if line != "":
            print(line)
        if "CPU halted" in line:
            raise RuntimeError(line)
        if time() - start_time > timeout_s:
            raise RuntimeError('Timeout')

        unity_done = re.search(unity_done_re, line)
        if unity_done:
            num_test_failures = int(unity_done.groups()[0])
            print('================ END OF DEVICE OUTPUT ================')
            for tr in test_results:
                print(tr)
            return num_test_failures

        if re.search(unity_test_end_re, line):
            test_results.append(line)

def run_ota_test(ser):
    ser.write('\r\n'.encode())
    wait_for_str_in_line(ser, 'esp32>')
    ser.write('start_ota\r\n'.encode())
    wait_for_str_in_line(ser, 'State = Downloading')
    wait_for_str_in_line(ser, 'Erasing flash')
    wait_for_str_in_line(ser, 'Received response')

    # At this point the download is proceeding, so wait up to 8 minutes for it to complete.
    wait_for_str_in_line(ser, 'Total bytes written', 480)

    wait_for_str_in_line(ser, 'State = Downloaded')
    wait_for_str_in_line(ser, 'State = Updating')
    wait_for_str_in_line(ser, 'Rebooting into new image')
    wait_for_str_in_line(ser, 'heap_init') # on new boot
    wait_for_str_in_line(ser, 'esp32>')
    ser.write('\r\n'.encode())
    wait_for_str_in_line(ser, 'esp32>')

    # Issue the start_ota command one last time to finalize the OTA process
    ser.write('start_ota\r\n'.encode())
    wait_for_str_in_line(ser, 'Firmware updated successfully!')

    # A final reset, to make sure OTA partitions are stable, no accidental rollbacks
    reset(ser)
    ser.write('start_ota\r\n'.encode())
    wait_for_str_in_line(ser, 'Manifest does not contain different firmware version')

def connect(ser):
    ser.write('\r\n'.encode())
    wait_for_str_in_line(ser, 'esp32>')
    ser.write('connect\r\n'.encode())
    wait_for_str_in_line(ser, 'Golioth connected')

def run_rpc_test(api_url, project_id, device_id, api_key):
    test_val = random.randint(1, 100)
    rpc = { "method":"double", "params":[test_val] }
    expected_return_val = test_val * 2

    full_url = "{}/v1/projects/{}/devices/{}/rpc".format(api_url, project_id, device_id)
    headers = { "Content-Type":"application/json", "x-api-key":api_key }
    response = requests.post(full_url, headers=headers, json=rpc)

    assert response.status_code == 200, response
    assert response.json()["detail"]["value"] == expected_return_val

def api_get_setting_id(api_url, project_id, api_key, setting_name):
    full_url = "{}/v1/projects/{}/settings".format(api_url, project_id)
    headers = { "x-api-key":api_key }
    response = requests.get(full_url, headers=headers)
    print(response.json())
    settings = response.json()["list"]

    for setting in settings:
        if setting["key"] == setting_name:
            return setting["id"]

    return None

def api_create_setting(api_url, project_id, api_key, setting_name, setting_val):
    full_url = "{}/v1/projects/{}/settings".format(api_url, project_id)
    headers = { "Content-Type":"application/json", "x-api-key":api_key }
    setting = {
            "key": setting_name,
            "dataType": "integer",
            "value": setting_val
    }
    return requests.post(full_url, headers=headers, json=setting)

def api_lightdb_get(api_url, project_id, device_id, api_key, ldb_path):
    full_url = "{}/v1/projects/{}/devices/{}/data/{}".format(
            api_url, project_id, device_id, ldb_path)
    headers = { "x-api-key":api_key }
    return requests.get(full_url, headers=headers)

def api_get_device_status(api_url, project_id, device_id, api_key):
    full_url = "{}/v1/projects/{}/devices/{}".format(api_url, project_id, device_id)
    headers = { "x-api-key":api_key }
    return requests.get(full_url, headers=headers)

def api_delete_setting(api_url, project_id, api_key, setting_id):
    if not setting_id:
        return None

    full_url = "{}/v1/projects/{}/settings/{}".format(
            api_url, project_id, setting_id)
    headers = { "x-api-key":api_key }
    return requests.delete(full_url, headers=headers)

def run_settings_test(api_url, project_id, device_id, api_key):
    setting_name = "TEST_SETTING"
    setting_val = random.randint(1, 100)
    print("setting_val", setting_val)

    # API: Delete test setting (if it exists)
    setting_id = api_get_setting_id(api_url, project_id, api_key, setting_name)
    delete = api_delete_setting(api_url, project_id, api_key, setting_id)
    print("first delete:", delete)

    # API: Create test setting, value random
    create = api_create_setting(api_url, project_id, api_key, setting_name, setting_val)
    assert create.status_code == 200, print(create)

    # Wait some time for changes to propagate to cloud database
    sleep(10)

    # API: Read LightDB, verify random value matches
    ldb_data = api_lightdb_get(api_url, project_id, device_id, api_key, setting_name)
    print("ldb_get:", ldb_data)

    # API: Get device settings status
    status = api_get_device_status(api_url, project_id, device_id, api_key)
    sync_status = status.json()["data"]["metadata"]["lastSettingsStatus"]["status"]
    print("status:", status)

    # API: Delete test setting
    setting_id = api_get_setting_id(api_url, project_id, api_key, setting_name)
    delete2 = api_delete_setting(api_url, project_id, api_key, setting_id)
    print("second delete:", delete2)

    assert ldb_data.status_code == 200, print(ldb_data)
    assert ldb_data.json()["data"] == setting_val, "expected {}, got {}".format(
            setting_val, ldb_data.json()["data"])

    # Note: not worrying about sync_status == "out-of-sync".
    # This can happen normally if there are other settings defined
    # in the project (other than TEST_SETTING).

def main():
    if len(sys.argv) != 2:
        print('usage: {} <port>'.format(sys.argv[0]))
        sys.exit(-1)
    port = sys.argv[1]

    # Connect to the device over serial and use the shell CLI to interact and run tests
    ser = serial.Serial(port, 115200, timeout=1)

    with open('credentials.yml', 'r') as f:
        credentials = yaml.safe_load(f)

    # Set WiFi and Golioth credentials over device shell CLI
    set_settings(ser, credentials["settings"])
    reset(ser)

    num_test_failures = 0

    # Run built in tests on the device and check output
    for _ in range(1):
        num_test_failures += run_built_in_tests(ser)
        if num_test_failures != 0:
            break
        reset(ser)

    api_url = credentials["golioth_api"]["api_url"]
    proj_id = credentials["golioth_api"]["project_id"]
    dev_id = credentials["golioth_api"]["device_id"]
    api_key = credentials["golioth_api"]["api_key"]

    # Connect device again, since we rebooted at the end of built-in-tests
    connect(ser)

    run_rpc_test(api_url, proj_id, dev_id, api_key)
    run_settings_test(api_url, proj_id, dev_id, api_key)

    if num_test_failures == 0:
        run_ota_test(ser)

    if num_test_failures == 0:
        green_print('---------------------------')
        print('')
        green_print('  ✓ All Tests Passed 🎉')
        print('')
        green_print('---------------------------')
    else:
        red_print('---------------------------')
        print('')
        red_print('  ✗ Failed {} Tests'.format(num_test_failures))
        print('')
        red_print('---------------------------')

    sys.exit(num_test_failures)

if __name__ == "__main__":
    main()
