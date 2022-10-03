################################################################################
# \file app.mk
# \version 1.0
#
# \brief
# Configuration file for adding/removing source files. Modify this file
# to suit the application needs.
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

-include ./libs/mtb.mk

MCUBOOT_PATH=$(SEARCH_mcuboot)

################################################################################
# MBEDTLS Files
################################################################################

MBEDTLS_PATH=$(MCUBOOT_PATH)/ext/mbedtls

SOURCES+=$(wildcard $(MBEDTLS_PATH)/library/*.c)\

INCLUDES+=\
     $(MBEDTLS_PATH)/include\
     $(MBEDTLS_PATH)/library\

################################################################################
# MCUboot Files
################################################################################

MCUBOOT_CY_PATH=$(MCUBOOT_PATH)/boot/cypress
MCUBOOTAPP_PATH=$(MCUBOOT_CY_PATH)/MCUBootApp

SOURCES+=\
    $(wildcard $(MCUBOOT_PATH)/boot/bootutil/src/*.c)\
    $(wildcard $(MCUBOOT_CY_PATH)/cy_flash_pal/flash_psoc6/cy_flash_map.c)\
    $(MCUBOOT_CY_PATH)/libs/retarget_io_pdl/cy_retarget_io_pdl.c\
    $(MCUBOOT_CY_PATH)/libs/watchdog/watchdog.c\
    $(MCUBOOTAPP_PATH)/cy_security_cnt.c\
    $(MCUBOOTAPP_PATH)/keys.c\
    $(MCUBOOTAPP_PATH)/cy_serial_flash_prog.c

# Do not include QSPI API from flash PAL when external flash is not used.
ifeq ($(USE_EXTERNAL_FLASH), 1)
SOURCES+=\
    $(wildcard $(MCUBOOT_CY_PATH)/cy_flash_pal/flash_psoc6/cy_smif_psoc6.c)\
    $(wildcard $(MCUBOOT_CY_PATH)/cy_flash_pal/flash_psoc6/flash_qspi/*.c)
endif

INCLUDES+=\
    ./keys\
    $(MCUBOOT_PATH)/boot/bootutil/include\
    $(MCUBOOT_PATH)/boot/bootutil/src\
    $(MCUBOOT_CY_PATH)/cy_flash_pal\
    $(MCUBOOT_CY_PATH)/cy_flash_pal/sysflash\
    $(MCUBOOT_CY_PATH)/cy_flash_pal/flash_psoc6/include\
    $(MCUBOOT_CY_PATH)/cy_flash_pal/flash_psoc6/flash_qspi\
    $(MCUBOOT_CY_PATH)/cy_flash_pal/flash_psoc6/include/flash_map_backend\
    $(MCUBOOT_CY_PATH)/libs/retarget_io_pdl\
    $(MCUBOOTAPP_PATH)\
    $(MCUBOOTAPP_PATH)/os\
    ./config\
    $(MCUBOOT_CY_PATH)/libs/watchdog
