################################################################################
# \file Makefile
# \version 1.0
#
# \brief
# Top-level application make file.
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

################################################################################
# Basic Configuration
################################################################################

# Target board/hardware (BSP).
# To change the target, it is recommended to use the Library manager
# ('make modlibs' from command line), which will also update Eclipse IDE launch
# configurations. If TARGET is manually edited, ensure TARGET_<BSP>.mtb with a
# valid URL exists in the application, run 'make getlibs' to fetch BSP contents
# and update or regenerate launch configurations for your IDE.
TARGET=CY8CPROTO-062-4343W

# Name of application (used to derive name of final linked file).
#
# If APPNAME is edited, ensure to update or regenerate launch
# configurations for your IDE.
APPNAME=golioth_app

# Name of toolchain to use. Options include:
#
# GCC_ARM -- GCC provided with ModusToolbox IDE
# ARM     -- ARM Compiler (must be installed separately)
# IAR     -- IAR Compiler (must be installed separately)
#
# See also: CY_COMPILER_PATH below
TOOLCHAIN=GCC_ARM

# Default build configuration. Options include:
#
# Debug -- build with minimal optimizations, focus on debugging.
# Release -- build with full optimizations
# Custom -- build with custom configuration, set the optimization flag in CFLAGS
#
# If CONFIG is manually edited, ensure to update or regenerate launch configurations
# for your IDE.
CONFIG=Debug

# If set to "true" or "1", display full command-lines when building.
VERBOSE=


################################################################################
# MCUboot Specific Configuration
###############################################################################

# Include the application make files
include ../bootloader_cm0p/shared_config.mk

# Python path definition
ifeq ($(OS),Windows_NT)
PYTHON_PATH?=python
else
PYTHON_PATH?=python3
endif

.PHONY: flashmap.mk

# Python command to generate flashmap header file from flashmap JSON file
ifneq ($(FLASH_MAP), )
flashmap.mk:
	@echo -e "\n==============================================================================================================================================================="
	@echo -e "= Generating flashmap.mk using $(FLASH_MAP) file (Look out for any warnings or errors in this step) ="
	@echo -e "==============================================================================================================================================================="
	$(PYTHON_PATH) ../bootloader_cm0p/scripts/flashmap.py -p $(PLATFORM) -i ../bootloader_cm0p/flashmap/$(FLASH_MAP) -o ./source/cy_flash_map.h -d 1 > ./flashmap.mk
	@echo -e "===============================================================================================================================================================\n"

-include ./flashmap.mk
endif

# Supported values: BOOT, UPGRADE
# The Golioth app defines the LED period differently depending on whether the
# image is BOOT or UPGRADE.
# BOOT: Use when image is built for the primary slot.
#       --pad argument is not passed to the imgtool.
# UPGRADE: Use when image is built for secondary slot.
#          --pad argument is passed to the imgtool.
IMG_TYPE ?= BOOT

# Starting address of the CM4 app or the offset at which the header of an image
# will begin. Image = Header + App + TLV + Trailer. See MCUboot documenation for
# details.
# New relocated address = ORIGIN + HEADER_OFFSET
ifeq ($(IMG_TYPE), UPGRADE)
HEADER_OFFSET ?= $(shell expr $$(( $(SECONDARY_IMG_START) - $(PRIMARY_IMG_START) )) )
else
HEADER_OFFSET ?= 0
endif

# Override the value from flashmap.mk
ifeq ($(USE_XIP), 0)
ifeq ($(IMG_TYPE), BOOT)
USE_EXTERNAL_FLASH = 0
endif
endif

# Path of the key file passed as a parameter to the Python module "imgtool" for
# signing the image. Name of the key file is set in bootloader_cm0p/shared_config.mk
# as a common parameter to both the Bootloader app and Golioth app.
KEY_FILE_PATH=../bootloader_cm0p/keys

# Set the version of the app using the following three variables.
# This version information is passed to the Python module "imgtool" while
# signing the image in the post build step. Default values are set as follows.
ifeq ($(IMG_TYPE), BOOT)
APP_VERSION_MAJOR ?= 1
APP_VERSION_MINOR ?= 0
APP_VERSION_BUILD ?= 0
else
ifeq ($(IMG_TYPE), UPGRADE)
APP_VERSION_MAJOR ?= 2
APP_VERSION_MINOR ?= 0
APP_VERSION_BUILD ?= 0
endif
endif

################################################################################
# Advanced Configuration
################################################################################

# Include the mtb.mk file
-include ./libs/mtb.mk

MCUBOOT_PATH=$(SEARCH_mcuboot)
MCUBOOT_CY_PATH=$(MCUBOOT_PATH)/boot/cypress
MCUBOOTAPP_PATH=$(MCUBOOT_CY_PATH)/MCUBootApp

GOLIOTHSDK_PATH=../../../..

# Enable optional code that is ordinarily disabled by default.
#
# Available components depend on the specific targeted hardware and firmware
# in use. In general, if you have
#
#    COMPONENTS=foo bar
#
# ... then code in directories named COMPONENT_foo and COMPONENT_bar will be
# added to the build
#
COMPONENTS=FREERTOS LWIP MBEDTLS

# Like COMPONENTS, but disable optional code that was enabled by default.
DISABLE_COMPONENTS=

# By default the build system automatically looks in the Makefile's directory
# tree for source code and builds it. The SOURCES variable can be used to
# manually add source code to the build process from a location not searched
# by default, or otherwise not found by the build system.
SOURCES+=$(wildcard $(MCUBOOT_PATH)/boot/bootutil/src/*.c)\
         $(wildcard $(MCUBOOT_CY_PATH)/libs/watchdog/*.c)\
         $(wildcard $(MCUBOOT_CY_PATH)/cy_flash_pal/flash_psoc6/*.c)\
         $(wildcard $(MCUBOOT_CY_PATH)/platforms/$(FAMILY)/img_confirm/*.c)

ifeq ($(USE_EXTERNAL_FLASH), 1)
SOURCES+=\
    $(wildcard $(MCUBOOT_CY_PATH)/cy_flash_pal/flash_psoc6/cy_smif_psoc6.c)\
    $(wildcard $(MCUBOOT_CY_PATH)/cy_flash_pal/flash_psoc6/flash_qspi/*.c)
endif

SOURCES+=\
    $(filter-out $(GOLIOTHSDK_PATH)/src/zcbor_any_skip_fixed.c, $(wildcard $(GOLIOTHSDK_PATH)/src/*.c))\
    $(wildcard $(GOLIOTHSDK_PATH)/port/freertos/*.c)\
    $(wildcard $(GOLIOTHSDK_PATH)/port/modus_toolbox/*.c)\
    $(wildcard $(GOLIOTHSDK_PATH)/port/modus_toolbox/libcoap/*.c)\
    $(wildcard $(GOLIOTHSDK_PATH)/examples/common/*.c)\
    $(GOLIOTHSDK_PATH)/external/heatshrink/src/heatshrink_decoder.c\
    $(GOLIOTHSDK_PATH)/external/miniz/miniz_tinfl.c\
    $(GOLIOTHSDK_PATH)/external/bsdiff/bspatch.c\
    $(GOLIOTHSDK_PATH)/external/zcbor/src/zcbor_common.c\
    $(GOLIOTHSDK_PATH)/external/zcbor/src/zcbor_decode.c\
    $(GOLIOTHSDK_PATH)/external/zcbor/src/zcbor_encode.c

# Like SOURCES, but for include directories. Value should be paths to
# directories (without a leading -I).
INCLUDES+=\
    ../bootloader_cm0p/keys\
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
    ../bootloader_cm0p/config\
    $(MCUBOOT_CY_PATH)/libs/watchdog

INCLUDES+=\
    $(GOLIOTHSDK_PATH)/src/include\
    $(GOLIOTHSDK_PATH)/src/priv_include\
    $(GOLIOTHSDK_PATH)/port/modus_toolbox\
    $(GOLIOTHSDK_PATH)/port/modus_toolbox/libcoap/include\
    $(GOLIOTHSDK_PATH)/external/heatshrink/include\
    $(GOLIOTHSDK_PATH)/external/heatshrink/src\
    $(GOLIOTHSDK_PATH)/external/miniz\
    $(GOLIOTHSDK_PATH)/port/common/miniz\
    $(GOLIOTHSDK_PATH)/external/bsdiff\
    $(GOLIOTHSDK_PATH)/external/zcbor/include\
    $(GOLIOTHSDK_PATH)/examples/common

# Add additional defines to the build process (without a leading -D).
MBEDTLSFLAGS = MBEDTLS_USER_CONFIG_FILE='"mbedtls_user_config.h"'
DEFINES=CYBSP_WIFI_CAPABLE CY_RTOS_AWARE CY_RETARGET_IO_CONVERT_LF_TO_CRLF $(PLATFORM) $(MBEDTLSFLAGS)
DEFINES+=APP_VERSION_MAJOR=$(APP_VERSION_MAJOR) \
         APP_VERSION_MINOR=$(APP_VERSION_MINOR) \
         APP_VERSION_BUILD=$(APP_VERSION_BUILD) \
         USER_APP_START=$(PRIMARY_IMG_START) \
         USER_APP_SIZE=$(SLOT_SIZE)
DEFINES+=CONFIG_GOLIOTH_USER_CONFIG_INCLUDE='"golioth_user_config.h"'
DEFINES+=HEATSHRINK_DYNAMIC_ALLOC=0

# Add defines to enable usage of external flash for secondary or both images (XIP)
ifeq ($(USE_EXTERNAL_FLASH), 1)
ifeq ($(USE_XIP), 1)
DEFINES+=USE_XIP
endif
DEFINES+=CY_BOOT_USE_EXTERNAL_FLASH
DEFINES+=CY_MAX_EXT_FLASH_ERASE_SIZE=$(PLATFORM_CY_MAX_EXT_FLASH_ERASE_SIZE)
endif

ifeq ($(USE_XIP), 1)
DEFINES+=USE_XIP
LD_SUFFIX=_xip
endif

DEFINES+=PSOC6

# Select softfp or hardfp floating point. Default is softfp.
VFP_SELECT=

# Additional / custom C compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
CFLAGS=

# Additional / custom C++ compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
CXXFLAGS=

# Additional / custom assembler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
ASFLAGS=

# Additional / custom linker flags.
LDFLAGS=

# Additional / custom libraries to link in to the application.
LDLIBS=

# Custom pre-build commands to run.
PREBUILD=

# Path to the linker script to use and other linker options.
ifeq ($(TOOLCHAIN), GCC_ARM)
LINKER_SCRIPT=$(wildcard ./linker_script/TARGET_$(TARGET)/TOOLCHAIN_$(TOOLCHAIN)/*cm4$(LD_SUFFIX).ld)
LDFLAGS+=-Wl,--defsym=MCUBOOT_HEADER_SIZE=$(MCUBOOT_HEADER_SIZE)
LDFLAGS+=-Wl,--defsym=CM0P_FLASH_SIZE=$(BOOTLOADER_SIZE),--defsym=CM0P_RAM_SIZE=$(BOOTLOADER_APP_RAM_SIZE)
LDFLAGS+=-Wl,--defsym=USER_APP_START=$(PRIMARY_IMG_START),--defsym=USER_APP_SIZE=$(SLOT_SIZE)

CY_ELF_TO_HEX_TOOL=$(CY_CROSSPATH)/bin/arm-none-eabi-objcopy
CY_ELF_TO_HEX_OPTIONS=-O ihex
else
$(error Only GCC_ARM is supported at this moment)
endif

CY_HEX_TO_BIN_TOOL="$(CY_CROSSPATH)/bin/arm-none-eabi-objcopy"

ifeq ($(USE_EXTERNAL_FLASH), 1)
ERASED_VALUE=0xff
else
ERASED_VALUE=0
endif

# add flag to imgtool if not using swap for upgrade
ifeq ($(USE_OVERWRITE), 1)
UPGRADE_TYPE:=--overwrite-only
DEFINES+=SWAP_DISABLED=1
else
DEFINES+=SWAP_DISABLED=0
endif

# Set build directory for BOOT and UPGRADE images
CY_BUILD_LOCATION=./build/$(IMG_TYPE)
BINARY_OUT_PATH=$(CY_BUILD_LOC)/$(TARGET)/$(CONFIG)/$(APPNAME)
IMGTOOL_PATH=$(MCUBOOT_PATH)/scripts/imgtool.py
CY_BUILD_VERSION=$(APP_VERSION_MAJOR).$(APP_VERSION_MINOR).$(APP_VERSION_BUILD)
SIGN_ARGS=sign --header-size $(MCUBOOT_HEADER_SIZE) --pad-header --align 1 -v $(CY_BUILD_VERSION) \
               -S $(SLOT_SIZE) -M $(MAX_IMG_SECTORS) $(UPGRADE_TYPE) -R $(ERASED_VALUE) -k $(KEY_FILE_PATH)/$(SIGN_KEY_FILE).pem

ifeq ($(IMG_TYPE), BOOT)
    DEFINES+=BOOT_IMAGE
else
ifeq ($(IMG_TYPE), UPGRADE)
    DEFINES+=UPGRADE_IMAGE
    SIGN_ARGS += --pad
else
    $(error Set IMG_TYPE to be either BOOT or UPGRADE)
endif
endif

# Python path definition
ifeq ($(OS),Windows_NT)
PYTHON_PATH?=python
else
PYTHON_PATH?=python3
endif

# Custom post-build commands to run.
# 1. Take a backup of the original hex file (_raw.hex)
# 2. Delete the original hex
# 3. Relocate the starting address based on HEADER_OFFSET and also convert
#    .elf to _unsigned.hex
# 4. Sign the image using imgtool (.hex)
#
# Step 3 is done so that programmer tools can place the image directly into
# secondary slot. This step is not required if an application (e.g. OTA) is
# going to write the image into secondary slot.
#
# Note that the final file should be $(APPNAME).hex so that Eclipse and
# Make CLI can locate the file for programming.
POSTBUILD=\
cp -f $(BINARY_OUT_PATH).hex $(BINARY_OUT_PATH)_raw.hex;\
rm -f $(BINARY_OUT_PATH).hex;\
$(CY_ELF_TO_HEX_TOOL) --change-addresses=$(HEADER_OFFSET) $(CY_ELF_TO_HEX_OPTIONS) $(BINARY_OUT_PATH).elf $(BINARY_OUT_PATH)_unsigned.hex;\
$(PYTHON_PATH) $(IMGTOOL_PATH) $(SIGN_ARGS) $(BINARY_OUT_PATH)_unsigned.hex $(BINARY_OUT_PATH).hex;\
$(CY_HEX_TO_BIN_TOOL) --input-target=ihex --output-target=binary $(BINARY_OUT_PATH).hex $(BINARY_OUT_PATH).bin


################################################################################
# Paths
################################################################################

# Relative path to the project directory (default is the Makefile's directory).
#
# This controls where automatic source code discovery looks for code.
CY_APP_PATH=

# Relative path to the shared repo location.
#
# All .mtb files have the format, <URI>#<COMMIT>#<LOCATION>. If the <LOCATION> field
# begins with $$ASSET_REPO$$, then the repo is deposited in the path specified by
# the CY_GETLIBS_SHARED_PATH variable. The default location is one directory level
# above the current app directory.
# This is used with CY_GETLIBS_SHARED_NAME variable, which specifies the directory name.
CY_GETLIBS_SHARED_PATH=../../

# Directory name of the shared repo location.
#
CY_GETLIBS_SHARED_NAME=mtb_shared

# Absolute path to the compiler's "bin" directory.
#
# The default depends on the selected TOOLCHAIN (GCC_ARM uses the ModusToolbox
# IDE provided compiler by default).
CY_COMPILER_PATH=


# Locate ModusToolbox IDE helper tools folders in default installation
# locations for Windows, Linux, and macOS.
CY_WIN_HOME=$(subst \,/,$(USERPROFILE))
CY_TOOLS_PATHS ?= $(wildcard \
    $(CY_WIN_HOME)/ModusToolbox/tools_* \
    $(HOME)/ModusToolbox/tools_* \
    /Applications/ModusToolbox/tools_*)

# If you install ModusToolbox IDE in a custom location, add the path to its
# "tools_X.Y" folder (where X and Y are the version number of the tools
# folder). Make sure you use forward slashes.
CY_TOOLS_PATHS+=

# Default to the newest installed tools folder, or the users override (if it's
# found).
CY_TOOLS_DIR=$(lastword $(sort $(wildcard $(CY_TOOLS_PATHS))))

ifeq ($(CY_TOOLS_DIR),)
$(error Unable to find any of the available CY_TOOLS_PATHS -- $(CY_TOOLS_PATHS). On Windows, use forward slashes.)
endif

$(info Tools Directory: $(CY_TOOLS_DIR))

include $(CY_TOOLS_DIR)/make/start.mk
