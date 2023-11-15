# ESP-IDF HIL Test Base

This provides a base ESP-IDF application that can be used to run HIL tests. The
application handles network connection and provides a shell interface for
setting network and Golioth (PSK) credentials. The application must be combined
with a test source file in order for it to compile correctly. The name of the
test (corresponding to the name of the folder containing the source file) can
be passed to the build system using the CMake variable `GOLIOTH_HIL_TEST`:

```sh
idf.py -D GOLIOTH_HIL_TEST=<test_name> set-target <target>
idf.py build
```

For example, to compile the connection HIL test for the ESP32-S3 from the
root of the SDK repository:

```sh
idf.py -D GOLIOTH_HIL_TEST=connection -C tests/hil/platform/esp-idf set-target esp32s3
idf.py -C tests/hil/platform/esp-idf build
```

The build is setup to produced a `merged.bin` binary that contains all the
necessary images to be flashed in one file, at offset 0x0.
