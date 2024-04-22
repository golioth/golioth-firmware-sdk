#!/bin/bash

declare -a zephyr_boards
# format: 'west_board_name shield_name'
# If there is no shield, then the a trailing space is required
zephyr_boards[0]='esp32_devkitc_wrover/esp32/procpu '
zephyr_boards[1]='frdm_rw612/rw612 '
zephyr_boards[2]='nrf52840dk/nrf52840 golioth_esp_at'
zephyr_boards[3]='rak5010/nrf52840 '

declare -a ncs_boards
# format: 'west_board_name'
ncs_boards[0]='nrf9160dk/nrf9160/ns'

if [ "${PWD##*/}" != "golioth-firmware-sdk" ]; then
    echo "You must run from the Golioth SDK root directory: golioth-firwmare-sdk"
    exit -1
fi

rm -rf ci_zephyr_artifacts
mkdir ci_zephyr_artifacts

### NCS
python -m west config manifest.file .ci-west-ncs.yml
python -m west update

for ncs_board in "${ncs_boards[@]}"
do
    rm -rf build
    python -m west build -b ${ncs_board} --sysbuild examples/zephyr/fw_update
    mv build/fw_update/zephyr/zephyr.signed.bin ci_zephyr_artifacts/ncs-${ncs_board//\//_}.bin
    python -m west build -b ${ncs_board} --sysbuild examples/zephyr/fw_update -- -DCONFIG_GOLIOTH_COAP_HOST_URI=\"coaps://coap.golioth.dev\"
    mv build/fw_update/zephyr/zephyr.signed.bin ci_zephyr_artifacts/ncs_devserver-${ncs_board//\//_}.bin
done

### Zephyr
python -m west config manifest.file .ci-west-zephyr.yml
python -m west update
python -m west blobs fetch hal_espressif

for zephyr_board in "${zephyr_boards[@]}"
do
    rm -rf build
    target=${zephyr_board%%\ *}
    shield=${zephyr_board##*\ }
    artifact_name=${target//\//_}
    python -m west build -b ${target} --sysbuild examples/zephyr/fw_update -- -Dfw_update_SHIELD="${shield}"
    mv build/fw_update/zephyr/zephyr.signed.bin ci_zephyr_artifacts/zephyr-${artifact_name}.bin
    python -m west build -b ${target} --sysbuild examples/zephyr/fw_update -- -DCONFIG_GOLIOTH_COAP_HOST_URI=\"coaps://coap.golioth.dev\" -Dfw_update_SHIELD="${shield}"
    mv build/fw_update/zephyr/zephyr.signed.bin ci_zephyr_artifacts/zephyr_devserver-${artifact_name}.bin
done
