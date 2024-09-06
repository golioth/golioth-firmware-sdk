# Build fw_update artifacts for Golioth hardware-in-the-loop tests

This branch includes changes and a script for building fw_update sample
artifacts used for our CI testing.

## Prerequsite

Rebase this branch on to the Golioth SDK version being used

```
git rebase -i v0.15.0
```

## ESP-IDF:

1. Prepare: update the `_sdk_version` in
   `examples/esp-idf/fw_update/main/app_main.c` to the Golioth Firmware
   SDK version used.
2. From `modules/lib/golioth` enable esp-idf and run the script:

    ```
    source <path_to_espidf>/esp-idf/export.sh
    bash -i scripts/ci_artifacts/generate_espidf.sh
    ```

## Zephyr:

1. Prepare: update the `CONFIG_GOLIOTH_CI_TESTING_SDK_VER` symbol in
   `examples/zephyr/fw_update/prj.conf` to the Golioth Firmware SDK
   version used.
2. From `modules/lib/golioth` enable the virtual environment and run the
   script:

    ```
    source <path_to_virual_env>/.env/bin/activate
    bash -i scripts/ci_artifacts/generate_zephyr.sh
    ```

## Upload artifacts to Golioth

1. Log into the console and select the `golioth-engineering/firmware_ci`
   project
2. Delete all artifacts that have the `255.8.9` version number
3. Upload each artifact from the `ci_espidf_artifacts` and
   `ci_zephyr_artifacts` directories
  1. These directories include both prod and dev arifacts, upload the
     artifact that does not have `devserver` in the title
  2. Choose the blueprint that matches the artifact you are uploading
  3. Set the version number to `255.8.9`

Repeat this process on the dev server (`golioth/ci` project), this time
uploading the artifacts the have `devserver` in the filename.

## Postrequisite

Commit changes on this branch and push to GitHub
