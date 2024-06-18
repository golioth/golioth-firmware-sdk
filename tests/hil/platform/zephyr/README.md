# Zephyr HIL Test Base

This provides a base Zephyr application that can be used to run HIL tests. The
application handles network connection and provides a shell interface for
setting network and Golioth (PSK) credentials. The application must be combined
with a test source file in order for it to compile correctly. The name of the
test (corresponding to the name of the folder containing the source file) can
be passed to the build system using the CMake variable `GOLIOTH_HIL_TEST`:

```sh
west build -p -b <board> . -- -DGOLIOTH_HIL_TEST=<test_name>
```

For example, to compile the connection HIL test for the nRF52840-DK from the
root of the SDK repository:

```sh
west build -p -b nrf52840dk/nrf52840 tests/hil/platform/zephyr -- -DGOLIOTH_HIL_TEST=connection
```

## Example of running Zephyr RPC test locally on nRF52840dk

```
cd modules/lib/golioth-firmware-sdk

# Dependencies
pip install tests/hil/scripts/pytest-hil/

# Build
west build -p -b nrf52840dk/nrf52840 tests/hil/platform/zephyr -- -DGOLIOTH_HIL_TEST=rpc

# Run test
pytest --rootdir . tests/hil/tests/rpc         \
  --board nrf52840dk                           \
  --port /dev/ttyACM0                          \
  --fw-image build/zephyr/zephyr.hex           \
  --serial-number 1050266122                   \
  --api-url "https://api.golioth.io"           \
  --api-key "i2u___GOLIOTHAPIKEY___uW2odN9avN" \
  --wifi-ssid "WiFiAPssid"                     \
  --wifi-psk "WifiAPpassword"                  \
  -v -s
```

The last two flags show verbose output and print statements
