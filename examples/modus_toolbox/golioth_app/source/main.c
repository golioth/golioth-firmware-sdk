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

#include "cybsp.h"
#include "cyhal.h"
#include "cy_retarget_io.h"
#include "watchdog.h"
#include <FreeRTOS.h>
#include "golioth_main.h"
#include <task.h>
#include <inttypes.h>

#ifdef CY_BOOT_USE_EXTERNAL_FLASH
#include "flash_qspi.h"
#endif

#define QSPI_SLAVE_SELECT_LINE              (1UL)

#define LED_TOGGLE_INTERVAL_MS              (1000u)

#define BLINK_TASK_STACK_SIZE               (3 * 1024)
#define BLINK_TASK_PRIORITY                 (1)

#define GOLIOTH_MAIN_TASK_STACK_SIZE        (5 * 1024)
#define GOLIOTH_MAIN_TASK_PRIORITY          (1)

static void blink_task(void* arg) {
    for (;;)
    {
        vTaskDelay(LED_TOGGLE_INTERVAL_MS / portTICK_PERIOD_MS);
        cyhal_gpio_toggle(CYBSP_USER_LED);
    }
}

/******************************************************************************
 * Function Name: main
 ******************************************************************************
 * Summary:
 *  System entrance point. This function initializes system resources &
 *  peripherals, initializes retarget IO, and toggles the user LED at a
 *  configured interval.
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
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);
    CY_ASSERT(result == CY_RSLT_SUCCESS);

#ifdef CY_BOOT_USE_EXTERNAL_FLASH
    cy_en_smif_status_t qspi_status = qspi_init_sfdp(QSPI_SLAVE_SELECT_LINE);
    if (CY_SMIF_SUCCESS == qspi_status) {
        printf("External Memory initialized w/ SFDP.");
    } else {
        printf("External Memory initialization w/ SFDP FAILED: 0x%08"PRIx32, (uint32_t)qspi_status);
    }
#endif

    /* Initialize the User LED */
    result = cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT,
              CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    (void) result; /* To avoid compiler warning in release build */

    printf("\n=========================================================\n");
    printf("[GoliothApp] Version: %d.%d.%d, CPU: CM4\n",
           APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD);
    printf("\n=========================================================\n");

    /* Update watchdog timer to mark successful start up of application */
    cy_wdg_kick();
    cy_wdg_free();
    printf("[GoliothApp] Watchdog timer started by the bootloader is now turned off to mark the successful start of Golioth app.\r\n");

    printf("[GoliothApp] User LED toggles at %d msec interval\r\n\n", LED_TOGGLE_INTERVAL_MS);

    /* Create the tasks. */
    xTaskCreate(blink_task, "Blink task", BLINK_TASK_STACK_SIZE, NULL,
                BLINK_TASK_PRIORITY, NULL);
    xTaskCreate(golioth_main_task, "Golioth main task", GOLIOTH_MAIN_TASK_STACK_SIZE, NULL,
                GOLIOTH_MAIN_TASK_PRIORITY, NULL);

    /* Start the FreeRTOS scheduler. */
    vTaskStartScheduler();

    /* Should never get here. */
    CY_ASSERT(0);

    for (;;)
    {
        /* Toggle the user LED periodically */
        cyhal_system_delay_ms(LED_TOGGLE_INTERVAL_MS);
        cyhal_gpio_toggle(CYBSP_USER_LED);
    }

    return 0;
}

/* [] END OF FILE */

