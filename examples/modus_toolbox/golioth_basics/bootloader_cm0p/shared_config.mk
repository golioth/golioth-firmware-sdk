################################################################################
# \file shared_config.mk
# \version 1.0
#
# \brief
# Holds configuration and error checking that are common to both the Bootloader
# and Blinky apps. Ensure that this file is included in the application's
# Makefile.
#
################################################################################
# \copyright
# Copyright 2020-2022, Cypress Semiconductor Corporation (an Infineon company)
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
################################################################################

# Flashmap JSON file name
FLASH_MAP=psoc62_overwrite_single_smif.json

# Device family name. Ex: PSOC6, CYW20289
FAMILY=PSOC6

# Device platform name (Empty for CYW20289 device).
# Ex: PSOC_062_2M, PSOC_062_1M, PSOC_062_512K
ifeq ($(TARGET), $(filter $(TARGET), CY8CKIT-062-WIFI-BT CY8CKIT-062-BLE CYW9P62S1-43438EVB-01 CYW9P62S1-43012EVB-01))
PLATFORM=PSOC_062_1M
endif

ifeq ($(TARGET), $(filter $(TARGET), CY8CPROTO-062-4343W CY8CKIT-062S2-43012 CY8CEVAL-062S2 CY8CEVAL-062S2-LAI-4373M2 CY8CEVAL-062S2-MUR-43439M2))
PLATFORM=PSOC_062_2M
endif

ifeq ($(TARGET), $(filter $(TARGET), CY8CPROTO-062S3-4343W))
PLATFORM=PSOC_062_512K
endif

# Name of the key file, used in two places.
# 1. #included in keys.c for embedding it in the Bootloader app, used for image
#    authentication.
# 2. Passed as a parameter to the Python module "imgtool" for signing the image
#    in the Blinky app Makefile. The path of this key file is set in the Blinky
#    app Makefile.
SIGN_KEY_FILE=cypress-test-ec-p256

# NOTE: Following variables are passed as options to the linker.
# Ensure that the values have no trailing white space. Linker will throw error
# otherwise.

# Flash and RAM size for MCUBoot Bootloader app run by CM0+
BOOTLOADER_APP_RAM_SIZE=0x20000

# MCUBoot header size
# Must be a multiple of 1024 because of the following reason.
# CM4 image starts right after the header and the CM4 image begins with the
# interrupt vector table. The starting address of the table must be 1024-bytes
# aligned. See README.md for details.
# Number of bytes to be aligned to = Number of interrupt vectors x 4 bytes.
# (1024 = 256 x 4)
#
# Header size is used in two places.
# 1. The location of CM4 image is offset by the header size from the ORIGIN
# value specified in the linker script.
# 2. Passed to the imgtool while signing the image. The imgtool fills the space
# of this size with zeroes and then adds the actual header from the beginning of
# the image.
MCUBOOT_HEADER_SIZE=0x400
