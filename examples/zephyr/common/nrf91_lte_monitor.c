/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lte_monitor);

#include <modem/lte_lc.h>
#include <zephyr/init.h>
#include <zephyr/sys/reboot.h>

static void lte_handler(const struct lte_lc_evt *const evt)
{
    switch (evt->type)
    {
        case LTE_LC_EVT_NW_REG_STATUS:
            switch (evt->nw_reg_status)
            {
                case LTE_LC_NW_REG_NOT_REGISTERED:
                    LOG_INF("Network: Not registered");
                    break;
                case LTE_LC_NW_REG_REGISTERED_HOME:
                    LOG_INF("Network: Registered (home)");
                    break;
                case LTE_LC_NW_REG_SEARCHING:
                    LOG_INF("Network: Searching");
                    break;
                case LTE_LC_NW_REG_REGISTRATION_DENIED:
                    LOG_INF("Network: Registration denied");
                    break;
                case LTE_LC_NW_REG_UNKNOWN:
                    LOG_INF("Network: Unknown");
                    break;
                case LTE_LC_NW_REG_REGISTERED_ROAMING:
                    LOG_INF("Network: Registered (roaming)");
                    break;
                case LTE_LC_NW_REG_UICC_FAIL:
                    LOG_INF("Network: UICC fail");
                    break;
            }
            break;
        case LTE_LC_EVT_RRC_UPDATE:
            switch (evt->rrc_mode)
            {
                case LTE_LC_RRC_MODE_CONNECTED:
                    LOG_INF("RRC: Connected");
                    break;
                case LTE_LC_RRC_MODE_IDLE:
                    LOG_INF("RRC: Idle");
                    break;
            }
            break;
        case LTE_LC_EVT_MODEM_EVENT:
            switch (evt->modem_evt)
            {
                case LTE_LC_MODEM_EVT_RESET_LOOP:
                    LOG_INF("Modem: Reset Loop detected");

#if defined(CONFIG_GOLIOTH_SAMPLE_NRF91_RESET_LOOP_OVERRIDE)
                    LOG_WRN("Attempting factory reset to override reset loop restriction");
                    lte_lc_offline();
                    lte_lc_factory_reset(LTE_LC_FACTORY_RESET_ALL);
                    sys_reboot(SYS_REBOOT_COLD);
#endif
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static int nrf91_lte_monitor_init(void)
{
    lte_lc_register_handler(lte_handler);
    lte_lc_modem_events_enable();

    return 0;
}

SYS_INIT(nrf91_lte_monitor_init, APPLICATION, 0);
