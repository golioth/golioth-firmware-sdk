#!/bin/bash

declare -a zephyr_boards
# format: 'west_board_name'
zephyr_boards[0]='esp32_devkitc_wrover/esp32/procpu'
zephyr_boards[1]='mimxrt1024_evk'
zephyr_boards[2]='nrf52840dk/nrf52840'
zephyr_boards[3]='rak5010/nrf52840'

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
python -m west config manifest.file west-ncs.yml
python -m west update

for ncs_board in "${ncs_boards[@]}"
do
    rm -rf build
    python -m west build -b ${ncs_board} examples/zephyr/fw_update
    mv build/zephyr/app_update.bin ci_zephyr_artifacts/ncs-${ncs_board//\//_}.bin
    python -m west build -b ${ncs_board} examples/zephyr/fw_update -- -DCONFIG_GOLIOTH_COAP_HOST_URI=\"coaps://coap.golioth.dev\"
    mv build/zephyr/app_update.bin ci_zephyr_artifacts/ncs_devserver-${ncs_board//\//_}.bin
done

### Zephyr
python -m west config manifest.file west-zephyr.yml
python -m west update

for zephyr_board in "${zephyr_boards[@]}"
do
    rm -rf build
    python -m west build -b ${zephyr_board} --sysbuild examples/zephyr/fw_update
    mv build/fw_update/zephyr/zephyr.signed.bin ci_zephyr_artifacts/zephyr-${zephyr_board//\//_}.bin
    python -m west build -b ${zephyr_board} --sysbuild examples/zephyr/fw_update -- -DCONFIG_GOLIOTH_COAP_HOST_URI=\"coaps://coap.golioth.dev\"
    mv build/fw_update/zephyr/zephyr.signed.bin ci_zephyr_artifacts/zephyr_devserver-${zephyr_board//\//_}.bin
done
