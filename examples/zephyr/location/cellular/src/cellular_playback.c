/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cellular_playback, LOG_LEVEL_DBG);

#include <zephyr/net/net_offload.h>

#include "cellular.h"

struct cellular_playback_info
{
    int64_t ts;
    const struct golioth_cellular_info *infos;
    size_t num_infos;
};

struct cellular_playback_config
{
    const struct cellular_playback_info *infos;
    size_t num_infos;
};

struct cellular_playback_data
{
    const struct cellular_playback_config *config;
    size_t info_idx;
};

static const struct cellular_playback_config cellular_playback_config_0;
static struct cellular_playback_data cellular_playback_data_0;

int cellular_info_get(struct golioth_cellular_info *infos,
                      size_t num_max_infos,
                      size_t *num_returned_infos)
{
    struct cellular_playback_data *data = &cellular_playback_data_0;
    const struct cellular_playback_config *config = data->config;
    const struct cellular_playback_info *playback_info = &config->infos[data->info_idx];
    size_t num_copied_infos = MIN(playback_info->num_infos, num_max_infos);

    k_sleep(
        K_TIMEOUT_ABS_MS(playback_info->ts * 1000 / CONFIG_GOLIOTH_CELLULAR_PLAYBACK_SPEED_FACTOR));

    memcpy(infos, playback_info->infos, num_copied_infos * sizeof(*infos));
    *num_returned_infos = num_copied_infos;

    data->info_idx++;

    return 0;
}

static int cellular_playback_init(void)
{
    const struct cellular_playback_config *config = &cellular_playback_config_0;
    struct cellular_playback_data *data = &cellular_playback_data_0;

    data->config = config;

    return 0;
}

#include "cellular_playback_input.c"

static const struct cellular_playback_config cellular_playback_config_0 = {
    .infos = cellular_infos,
    .num_infos = ARRAY_SIZE(cellular_infos),
};

SYS_INIT(cellular_playback_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
