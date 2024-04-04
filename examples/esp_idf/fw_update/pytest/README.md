# FW Update Test

This is a simple test that determines whether the DUT is able to connect with the
Golioth backend and perform Firmware Update. Before running this test, create a build
with _current_version as "255.8.9" in app_main.c and upload the binary fw_update.bin
on the Golioth console under "Upload Artifact" with Package ID as "main" and
Artifact Version as "255.8.9".

See esp_idf/README.md for test requirements and commands to run the test.
