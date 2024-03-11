# Firmware Update using pytest

This test requires that a pre-compiled firmware artifact be staged on
the Golioth server prior to the first run. This artifact needs
to be recompile and uploaded for each new release of the Golioth
Firmware SDK.

## How to compile and stage fw_update artifacts

### Update the ci artifact branch for a new reelase
1. Checkout the `dnd/ci-fw-update-artifacts`.
2. Rebase this branch on the new release of `main`.
3. Update `CONFIG_GOLIOTH_CI_TESTING_SDK_VER` in `prj.conf` using the
   new release number.
4. Make a new commit to this branch that includes these updates.

### Build firmware for each supported device

Build firmware for each supported device:

```
$ west build -b esp32_devkitc_wrover --sysbuild examples/zephyr/fw_update
$ west build -b nrf52840dk_nrf52840 --sysbuild examples/zephyr/fw_update
$ west build -b nrf9160dk_nrf9160_ns examples/zephyr/fw_update
$ west build -b mimxrt1024_evk --sysbuild examples/zephyr/fw_update
```

### Stage the artifacts

1. Log into the `golioth-engineering / firmware_ci` project on the
   Golioth Console.
2. Remove existing artifacts that have the `255.8.9` version number.
3. Upload artifacts:
    1. Select the blueprint that matches the artifact.
    2. Use version number `255.8.9`.
    3. For the nRF9160 upload `build/zephyr/app_update.bin`. For all
       other boards upload `build/fw_update/zephyr/zephyr.signed.bin`
