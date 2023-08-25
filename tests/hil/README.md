# Hardware-in-the-Loop Tests

This folder contains tests of the Golioth Firmware SDK that are designed to run
on hardware (as opposed to unit tests or simulations that run off-target on
host computers). Tests are split into platform specific code and test specific
code. The platform specific code provides a generic test base for each of the
supported platforms (e.g. Zephyr, ESP-IDF) that can run each of the various
tests. The test specific code is written using the `golioth_sys_*` abstractions
so that it can be combined with each platform test base to create a complete
test application.

## Platforms

The platform code is responsible for booting the board, setting up a network
connection, and providing a facility for the test runner to set network and
Golioth credentials (e.g. through a settings shell). After performing platform
specific setup, the application will call `hil_test_entry()`. This function
is defined by each of the tests (see below). The test to run is selected by
compiling different test files with the platform specific application. See
each platform's README for instructions on how to indicate the test to compile.

## Tests

Tests consist of firmware running on the DUT as well as a python script that
verifies the behavior of the DUT. Each test is in its own folder under the
`tests/` directory and contains (at least) a .c file and a python file with a
`test_` prefix in the name (required for the file to be recognized by pytest).
The .c file should provide a definition for the `hil_test_entry()` function,
which will be called by the platform specific firmware.
