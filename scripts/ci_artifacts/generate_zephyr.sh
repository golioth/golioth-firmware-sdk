#!/bin/bash

declare -a zephyr_boards
# format: 'west_board_name'
zephyr_boards[0]='esp32_devkitc_wrover'
zephyr_boards[1]='mimxrt1024_evk'
zephyr_boards[2]='nrf52840dk_nrf52840'
zephyr_boards[3]='rak5010_nrf52840'

declare -a ncs_boards
# format: 'west_board_name'
ncs_boards[0]='nrf9160dk_nrf9160_ns'

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
    mv build/zephyr/app_update.bin ci_zephyr_artifacts/ncs-${ncs_board}.bin
    python -m west build -b ${ncs_board} examples/zephyr/fw_update -- -DCONFIG_GOLIOTH_COAP_HOST_URI=\"coaps://coap.golioth.dev\"
    mv build/zephyr/app_update.bin ci_zephyr_artifacts/ncs_devserver-${ncs_board}.bin
done

### Zephyr
python -m west config manifest.file west-zephyr.yml
python -m west update

for zephyr_board in "${zephyr_boards[@]}"
do
    rm -rf build
    python -m west build -b ${zephyr_board} --sysbuild examples/zephyr/fw_update
    mv build/fw_update/zephyr/zephyr.signed.bin ci_zephyr_artifacts/zephyr-${zephyr_board}.bin
    python -m west build -b ${zephyr_board} --sysbuild examples/zephyr/fw_update -- -DCONFIG_GOLIOTH_COAP_HOST_URI=\"coaps://coap.golioth.dev\"
    mv build/fw_update/zephyr/zephyr.signed.bin ci_zephyr_artifacts/zephyr_devserver-${zephyr_board}.bin
done

#rm -rf build
#python -m west build -b nrf52840dk_nrf52840 --sysbuild examples/zephyr/fw_update
#mv build/fw_update/zephyr/zephyr.signed.bin ci_zephyr_artifacts/zephyr-nrf52840dk_nrf52840.bin
#python -m west build -b nrf52840dk_nrf52840 --sysbuild examples/zephyr/fw_update -- -DCONFIG_GOLIOTH_COAP_HOST_URI=\"coaps://coap.golioth.dev\"
#mv build/fw_update/zephyr/zephyr.signed.bin ci_zephyr_artifacts/zephyr_devserver-nrf52840dk_nrf52840.bin

#rm -rf build
#python -m west build -b mimxrt1024_evk --sysbuild examples/zephyr/fw_update
#mv build/fw_update/zephyr/zephyr.signed.bin ci_zephyr_artifacts/zephyr-mimxrt1024_evk.bin
#python -m west build -b mimxrt1024_evk --sysbuild examples/zephyr/fw_update -- -DCONFIG_GOLIOTH_COAP_HOST_URI=\"coaps://coap.golioth.dev\"
#mv build/fw_update/zephyr/zephyr.signed.bin ci_zephyr_artifacts/zephyr_devserver-mimxrt1024_evk.bin

#rm -rf build
#python -m west build -b rak5010_nrf52840 --sysbuild examples/zephyr/fw_update
#mv build/fw_update/zephyr/zephyr.signed.bin ci_zephyr_artifacts/zephyr-rak5010_nrf52840.bin
#python -m west build -b rak5010_nrf52840 --sysbuild examples/zephyr/fw_update -- -DCONFIG_GOLIOTH_COAP_HOST_URI=\"coaps://coap.golioth.dev\"
#mv build/fw_update/zephyr/zephyr.signed.bin ci_zephyr_artifacts/zephyr_devserver-rak5010_nrf52840.bin

#rm -rf build
