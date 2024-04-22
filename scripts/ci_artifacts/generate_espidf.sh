#!/bin/bash

declare -a boards
# format: 'board_name;target_chip'
boards[0]='esp32_devkitc_wrover;esp32'
boards[1]='esp32c3_devkitm;esp32c3'
boards[2]='esp32s3_devkitc;esp32s3'

if [ "${PWD##*/}" != "golioth-firmware-sdk" ]; then
    echo "You must run from the Golioth SDK root directory: golioth-firwmare-sdk"
    exit -1
fi

rm -rf ci_espidf_artifacts
mkdir ci_espidf_artifacts

### ESP-IDF
for board in "${boards[@]}"
do
    IFS=";" read -r -a arr <<< "${board}"
    board_name="${arr[0]}"
    target_name="${arr[1]}"

    rm -rf examples/esp_idf/fw_update/build
    rm examples/esp_idf/fw_update/sdkconfig
    idf.py -C examples/esp_idf/fw_update set-target ${target_name}
    idf.py -C examples/esp_idf/fw_update build
    mv examples/esp_idf/fw_update/build/fw_update.bin ci_espidf_artifacts/espidf-${board_name}.bin
    rm examples/esp_idf/fw_update/sdkconfig
    echo "CONFIG_GOLIOTH_COAP_HOST_URI=\"coaps://coap.golioth.dev\"" >> examples/esp_idf/fw_update/sdkconfig.defaults
    idf.py -C examples/esp_idf/fw_update build
    mv examples/esp_idf/fw_update/build/fw_update.bin ci_espidf_artifacts/espidf_devserver-${board_name}.bin
    git restore examples/esp_idf/fw_update/sdkconfig.defaults
    rm -rf examples/esp_idf/fw_update/build
    rm examples/esp_idf/fw_update/sdkconfig
done
