# Linux HIL Test Base

This provides a base Linux application that can be used to run HIL tests. The
application handles network connection and provides a shell interface for
setting Golioth (PSK) credentials. The application must be combinedwith a test
source file in order for it to compile correctly. The name of the test
(corresponding to the name of the folder containing the source file) can be
passed to the build system using the CMake variable `GOLIOTH_HIL_TEST`:

```sh
cmake . -B build -DGOLIOTH_HIL_TEST=<test_name>
make -C build
```

For example, to compile the connection HIL test from the root of the SDK
repository:

```sh
cmake -S tests/hil/platform/linux -B build -DGOLIOTH_HIL_TEST=connection
make -C build
```
