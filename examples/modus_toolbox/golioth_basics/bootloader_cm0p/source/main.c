/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the CE230650 - PSoC 6 MCU: MCUboot-
*              Based Basic Bootloader code example for ModusToolbox.
*
* Related Document: See README.md
*
*******************************************************************************
* Copyright 2020-2022, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/* Drive header files */
#include "cy_pdl.h"
#include "cycfg.h"
#include "cy_result.h"
#include "cy_retarget_io_pdl.h"

#include "cycfg_clocks.h"
#include "cycfg_peripherals.h"
#include "cycfg_pins.h"

/* Flash PAL header files */
#ifdef CY_BOOT_USE_EXTERNAL_FLASH
#include "flash_qspi.h"
#endif

/* MCUboot header files */
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil/sign_key.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/fault_injection_hardening.h"

/* Watchdog header file */
#include "watchdog.h"

/*******************************************************************************
* Macros
********************************************************************************/
/* Slave Select line to which the external memory is connected.
 * Acceptable values are:
 * 0 - SMIF disabled (no external memory)
 * 1, 2, 3, or 4 - slave select line to which the memory module is connected.
 */
#define QSPI_SLAVE_SELECT_LINE              (1UL)

/* WDT time out for reset mode, in milliseconds. */
#define WDT_TIME_OUT_MS                     (4000UL)

/* Number of attempts to check if UART TX is complete. 10ms delay is applied
 * between successive attempts.
 */
#define UART_TX_COMPLETE_POLL_COUNT         (10UL)

/* General MCUBoot module error */
#define CY_RSLT_MODULE_MCUBOOTAPP           (0x500U)
#define CY_RSLT_MODULE_MCUBOOTAPP_MAIN      (0x51U)
#define MCUBOOTAPP_RSLT_ERR \
    (CY_RSLT_CREATE_EX(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_MCUBOOTAPP, CY_RSLT_MODULE_MCUBOOTAPP_MAIN, 0))


/******************************************************************************
 * Function Name: deinit_hw
 ******************************************************************************
 * Summary:
 *  This function performs the necessary hardware de-initialization.
 *
 ******************************************************************************/
static void hw_deinit(void)
{
    cy_retarget_io_wait_tx_complete(CYBSP_UART_HW, 10);
    cy_retarget_io_pdl_deinit();
    Cy_GPIO_Port_Deinit(CYBSP_UART_RX_PORT);
    Cy_GPIO_Port_Deinit(CYBSP_UART_TX_PORT);
#if defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP) && !defined(USE_XIP)
    qspi_deinit(QSPI_SLAVE_SELECT_LINE);
#endif
}


/******************************************************************************
 * Function Name: do_boot
 ******************************************************************************
 * Summary:
 *  This function extracts the image address and enables CM4 to let it boot
 *  from that address.
 *
 * Parameters:
 *  rsp - Pointer to a structure holding the address to boot from.
 *
 ******************************************************************************/
static bool do_boot(struct boot_rsp *rsp)
{
    uintptr_t flash_base = 0;

    if (rsp != NULL)
    {
        int rc = flash_device_base(rsp->br_flash_dev_id, &flash_base);

        if (0 == rc)
        {
            fih_uint app_addr = fih_uint_encode(flash_base +
                                                rsp->br_image_off +
                                                rsp->br_hdr->ih_hdr_size);

            BOOT_LOG_INF("Starting User Application (wait)...");
            if (IS_ENCRYPTED(rsp->br_hdr))
            {
                BOOT_LOG_INF(" * User application is encrypted");
            }

            BOOT_LOG_INF("Start slot Address: 0x%08" PRIx32, (uint32_t)fih_uint_decode(app_addr));

            rc = flash_device_base(rsp->br_flash_dev_id, &flash_base);
            if ((rc != 0) || fih_uint_not_eq(fih_uint_encode(flash_base +
                                             rsp->br_image_off +
                                             rsp->br_hdr->ih_hdr_size),
                                             app_addr))
            {
                return false;
            }

            BOOT_LOG_INF("MCUBoot Bootloader finished");
            BOOT_LOG_INF("Deinitializing hardware...");

            hw_deinit();

#ifdef USE_XIP
            BOOT_LOG_INF("XIP: Switch to SMIF XIP mode");
            qspi_set_mode(CY_SMIF_MEMORY);
#endif

            Cy_SysEnableCM4(fih_uint_decode(app_addr));
            return true;
        }
        else
        {
            BOOT_LOG_ERR("Flash device ID not found");
            return false;
        }
    }

    return false;
}


/******************************************************************************
 * Function Name: main
 ******************************************************************************
 * Summary:
 *  System entrance point. This function initializes peripherals, initializes
 *  retarget IO, and performs a boot by calling the MCUboot functions.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  int
 *
 ******************************************************************************/
int main(void)
{
    struct boot_rsp rsp;
    cy_rslt_t rc = MCUBOOTAPP_RSLT_ERR;
    bool boot_succeeded = false;
    fih_int fih_rc = FIH_FAILURE;

/* Certain PSoC 6 devices enable CM4 by default at startup. It must be
 * either disabled or enabled & running a valid application for flash write
 * to work from CM0+. Since flash write may happen in boot_go() for updating
 * the image before this bootloader app can enable CM4 in do_boot(), we need
 * to keep CM4 disabled. Note that debugging of CM4 is not supported when it
 * is disabled.
 */
#if defined(CY_DEVICE_PSOC6ABLE2)
    if (CY_SYS_CM4_STATUS_ENABLED == Cy_SysGetCM4Status()) {
        Cy_SysDisableCM4();
    }
#endif

    /* Initialize system resources and peripherals. */
    init_cycfg_all();

    /* enable interrupts */
    __enable_irq();

    /* Initialize retarget-io to redirect the printf output */
    cy_retarget_io_pdl_init(CY_RETARGET_IO_BAUDRATE);

    BOOT_LOG_INF("MCUBoot Bootloader Started");

#ifdef CY_BOOT_USE_EXTERNAL_FLASH
    cy_en_smif_status_t qspi_status = qspi_init_sfdp(QSPI_SLAVE_SELECT_LINE);

    if (CY_SMIF_SUCCESS == qspi_status)
    {
        rc = CY_RSLT_SUCCESS;
        BOOT_LOG_INF("External Memory initialized w/ SFDP.");
    }
    else
    {
        rc = MCUBOOTAPP_RSLT_ERR;
        BOOT_LOG_ERR("External Memory initialization w/ SFDP FAILED: 0x%08" PRIx32, (uint32_t)qspi_status);
    }

#endif

    (void)memset(&rsp, 0, sizeof(rsp));
    FIH_CALL(boot_go, fih_rc, &rsp);

    if (true == fih_eq(fih_rc, FIH_SUCCESS))
    {

        BOOT_LOG_INF("User Application validated successfully");

        /* initialize watchdog timer. it should be updated from user app
        * to mark successful start up of this app. if the watchdog is not updated,
        * reset will be initiated by watchdog timer and swap revert operation started
        * to roll back to operable image.
        */
        rc = cy_wdg_init(WDT_TIME_OUT_MS);

        if (CY_RSLT_SUCCESS == rc)
        {
            boot_succeeded = do_boot(&rsp);

            if (!boot_succeeded)
            {
                BOOT_LOG_ERR("Boot of next app failed");
            }
        }
        else
        {
            BOOT_LOG_ERR("Failed to init WDT");
        }
    }
    else
    {
        BOOT_LOG_ERR("MCUBoot Bootloader found none of bootable images");
    }

    while (true)
    {
        if (boot_succeeded)
        {
            (void)Cy_SysPm_CpuEnterDeepSleep(CY_SYSPM_WAIT_FOR_INTERRUPT);
        }
        else
        {
            __WFI();
        }
    }
}

/* [] END OF FILE */

