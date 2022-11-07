# examples/test

Example app that runs integration tests on the target hardware.

The python script will connect to the device over serial and verify
the pass/fail result of each test.

The script will return with an exit code equal to the number of
failed tests (0 means all tests passed), or -1 for general error.

The verify.py script requires a file named `credentials.yml` (ignored by git) in
order to provision Wi-Fi and Golioth credentials to the device:

```yaml
settings:
    wifi/ssid: MyWiFiSSID
    wifi/psk: MyWiFiPassword
    golioth/psk-id: device@project
    golioth/psk: supersecret

golioth_api:
    api_url: https://api.golioth.io
    project_id: project
    device_id: <long_hex_number>
    api_key: <api_key>
```

The `golioth_api` object is used for the Settings and RPC test,
to access the Golioth REST API.

### Install Pre-Requisities

```sh
pip install pyyaml
```

### Run Test

To build, flash, and run the tests:

```sh
idf.py build && python flash.py && python verify.py /dev/ttyUSB0
```
