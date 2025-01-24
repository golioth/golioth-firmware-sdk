#!/usr/bin/env python3

import logging
from pathlib import Path
import pickle
import sys
import yaml

import click
from intelhex import bin2hex
import west.configuration

WEST_TOPDIR = Path(west.configuration.west_dir()).parent

sys.path.insert(0, str(WEST_TOPDIR / 'zephyr' / 'scripts' / 'dts' / 'python-devicetree' / 'src'))
sys.path.insert(0, str(WEST_TOPDIR / 'zephyr' / 'scripts' / 'west_commands'))

from devicetree.edtlib import Node
from runners.core import BuildConfiguration


LOGGER = logging.getLogger(__name__)


def lfs_partition(build_dir: Path) -> Node:
    edt_pickle = build_dir / 'zephyr' / 'edt.pickle'
    with open(edt_pickle, 'rb') as f:
        edt = pickle.load(f)

    fstab = edt.label2node["lfs1"]

    return fstab.props["partition"].val


@click.command()
@click.option("--build-dir", help="Zephyr build directory",
              type=click.Path(file_okay=False, path_type=Path))
def main(build_dir):
    build_conf = BuildConfiguration(str(build_dir))

    if "CONFIG_FLASH_BASE_ADDRESS" in build_conf:
        flash_base = build_conf["CONFIG_FLASH_BASE_ADDRESS"]
    else:
        flash_base = 0

    part = lfs_partition(build_dir)
    flash = part.parent.parent
    part_addr = part.regs[0].addr
    block_size = flash.props["erase-block-size"].val

    if "CONFIG_PARTITION_MANAGER_ENABLED" in build_conf:
        # Special case for NCS Partition Manager
        partitions_yml_path = build_dir.parent / "partitions.yml"
        partitions = yaml.load(partitions_yml_path.read_text(), Loader=yaml.Loader)
        lfs_info = partitions["littlefs_storage"]
        part_addr = lfs_info["address"]

    LOGGER.info("Block size: %s", block_size)
    LOGGER.info("LFS flash offset: %s", hex(flash_base))
    LOGGER.info("LFS partition offset: %s", hex(part_addr))

    # Override first 2 blocks to make sure LittleFS is reformatted
    with open(build_dir / "lfs.bin", "wb") as fh:
        fh.write(b"\xff" * block_size * 2)

    # Generate HEX file from BIN
    offset = flash_base + part_addr
    bin2hex(str(build_dir / "lfs.bin"),
            str(build_dir / "lfs.hex"),
            offset)


if __name__ == '__main__':
    main()
