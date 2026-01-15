/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cellular_nrf91);

#include <modem/lte_lc.h>

#include "cellular.h"

/* Indicates when individual ncellmeas operation is completed. This is internal to this file. */
static K_SEM_DEFINE(scan_cellular_sem_ncellmeas_evt, 0, 1);

static enum lte_lc_lte_mode lte_mode;
static struct lte_lc_ncell neighbor_cells[CONFIG_LTE_NEIGHBOR_CELLS_MAX];
static struct lte_lc_cells_info scan_cellular_info = {
    .neighbor_cells = neighbor_cells,
};

static const char *lte_mode_to_str(enum lte_lc_lte_mode mode)
{
    switch (mode)
    {
        case LTE_LC_LTE_MODE_NONE:
            return "NONE";
        case LTE_LC_LTE_MODE_LTEM:
            return "LTEM";
        case LTE_LC_LTE_MODE_NBIOT:
            return "NBIOT";
        case LTE_LC_LTE_MODE_NTN_NBIOT:
            return "NTN_NBIOT";
    }

    return "unknown";
}

static void lte_ind_handler(const struct lte_lc_evt *const evt)
{
    switch (evt->type)
    {
        case LTE_LC_EVT_LTE_MODE_UPDATE:
            LOG_INF("LTE mode: %s", lte_mode_to_str(evt->lte_mode));
            lte_mode = evt->lte_mode;
            break;
        case LTE_LC_EVT_CELL_UPDATE:
            LOG_INF("LTE cell changed: Cell ID: %d, Tracking area: %d",
                    evt->cell.id,
                    evt->cell.tac);
            break;
        case LTE_LC_EVT_NEIGHBOR_CELL_MEAS:
            LOG_INF("LTE current cell: MCC %d, MNC %d, Cell ID: %d, Tracking area: %d",
                    evt->cells_info.current_cell.mcc,
                    evt->cells_info.current_cell.mnc,
                    evt->cells_info.current_cell.id,
                    evt->cells_info.current_cell.tac);

            LOG_INF(
                "Cell measurements results received: "
                "ncells_count=%d, gci_cells_count=%d, current_cell.id=0x%08X",
                evt->cells_info.ncells_count,
                evt->cells_info.gci_cells_count,
                evt->cells_info.current_cell.id);

            if (evt->cells_info.current_cell.id != LTE_LC_CELL_EUTRAN_ID_INVALID)
            {
                /* Copy current cell information. We are seeing this is not set for GCI
                 * search sometimes but we have it for the previous normal neighbor search.
                 */
                memcpy(&scan_cellular_info.current_cell,
                       &evt->cells_info.current_cell,
                       sizeof(struct lte_lc_cell));
            }

            /* Copy neighbor cell information if present */
            if (evt->cells_info.ncells_count > 0 && evt->cells_info.neighbor_cells)
            {
                memcpy(scan_cellular_info.neighbor_cells,
                       evt->cells_info.neighbor_cells,
                       sizeof(struct lte_lc_ncell) * evt->cells_info.ncells_count);

                scan_cellular_info.ncells_count = evt->cells_info.ncells_count;
            }
            else
            {
                LOG_DBG("No neighbor cell information from modem");
            }

            /* Copy surrounding cell information if present */
            if (evt->cells_info.gci_cells_count > 0 && evt->cells_info.gci_cells)
            {
                memcpy(scan_cellular_info.gci_cells,
                       evt->cells_info.gci_cells,
                       sizeof(struct lte_lc_cell) * evt->cells_info.gci_cells_count);

                scan_cellular_info.gci_cells_count = evt->cells_info.gci_cells_count;
            }
            else
            {
                LOG_DBG("No surrounding cell information from modem");
            }

            k_sem_give(&scan_cellular_sem_ncellmeas_evt);
            break;

        default:
            break;
    }
}

int cellular_info_get(struct golioth_cellular_info *infos,
                      size_t num_max_infos,
                      size_t *num_returned_infos)
{
    struct lte_lc_ncellmeas_params params = {
        .search_type = LTE_LC_NEIGHBOR_SEARCH_TYPE_EXTENDED_LIGHT,
    };
    struct lte_lc_cell *current = &scan_cellular_info.current_cell;
    int err;

    err = lte_lc_neighbor_cell_measurement(&params);
    if (err)
    {
        return err;
    }

    err = k_sem_take(&scan_cellular_sem_ncellmeas_evt, K_FOREVER);
    if (err)
    {
        /* Semaphore was reset so stop search procedure */
        err = 0;
    }

    switch (lte_mode)
    {
        case LTE_LC_LTE_MODE_LTEM:
            infos[0].type = GOLIOTH_CELLULAR_TYPE_LTECATM;
            break;
        case LTE_LC_LTE_MODE_NBIOT:
            infos[0].type = GOLIOTH_CELLULAR_TYPE_NBIOT;
            break;
        default:
            *num_returned_infos = 0;
            return 0;
    }

    infos[0].mcc = current->mcc;
    infos[0].mnc = current->mnc;
    infos[0].id = current->id;

    *num_returned_infos = 1;

    return 0;
}

static int cellular_nrf91_init(void)
{
    lte_lc_register_handler(lte_ind_handler);

    return 0;
}

SYS_INIT(cellular_nrf91_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
