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
west build -p -b nrf52840dk_nrf52840 tests/hil/platform/zephyr -- -DGOLIOTH_HIL_TEST=connection
```
