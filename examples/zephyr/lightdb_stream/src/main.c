/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lightdb_stream, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/lightdb_stream.h>
#include <samples/common/sample_credentials.h>
#include <samples/common/net_connect.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

struct golioth_client *client;
static K_SEM_DEFINE(connected, 0, 1);

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg) {
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected) {
        k_sem_give(&connected);
    }
    LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}

#if DT_NODE_HAS_STATUS(DT_ALIAS(temp0), okay)

static int get_temperature(struct sensor_value *val) {
    const struct device *dev = DEVICE_DT_GET(DT_ALIAS(temp0));
    static const enum sensor_channel temp_channels[] = {
        SENSOR_CHAN_AMBIENT_TEMP,
        SENSOR_CHAN_DIE_TEMP,
    };
    int i;
    int err;

    err = sensor_sample_fetch(dev);
    if (err) {
        LOG_ERR("Failed to fetch temperature sensor: %d", err);
        return err;
    }

    for (i = 0; i < ARRAY_SIZE(temp_channels); i++) {
        err = sensor_channel_get(dev, temp_channels[i], val);
        if (err == -ENOTSUP) {
            /* try next channel */
            continue;
        } else if (err) {
            LOG_ERR("Failed to get temperature: %d", err);
            return err;
        }
    }

    return err;
}

#else

static int get_temperature(struct sensor_value *val) {
    static int counter = 0;

    /* generate a temperature from 20 deg to 30 deg, with 0.5 deg step */

    val->val1 = 20 + counter / 2;
    val->val2 = counter % 2 == 1 ? 500000 : 0;

    counter = (counter + 1) % 20;

    return 0;
}

#endif

static void temperature_push_handler(struct golioth_client *client,
                                     const struct golioth_response *response,
                                     const char *path,
                                     void *arg) {
    if (response->status != GOLIOTH_OK) {
        LOG_WRN("Failed to push temperature: %d", response->status);
        return;
    }

    LOG_DBG("Temperature successfully pushed");

    return;
}

static void temperature_push_async(const struct sensor_value *temp) {
    char sbuf[sizeof("{\"temp\":-4294967295.123456}")];
    int err;

    snprintk(sbuf, sizeof(sbuf), "{\"temp\":%d.%06d}", temp->val1, abs(temp->val2));

    err = golioth_lightdb_stream_set_json_async(client,
                                                "sensor",
                                                sbuf,
                                                strlen(sbuf),
                                                temperature_push_handler,
                                                NULL);
    if (err) {
        LOG_WRN("Failed to push temperature: %d", err);
        return;
    }
}

static void temperature_push_sync(const struct sensor_value *temp) {
    char sbuf[sizeof("{\"temp\":-4294967295.123456}")];
    int err;

    snprintk(sbuf, sizeof(sbuf), "{\"temp\":%d.%06d}", temp->val1, abs(temp->val2));

    err = golioth_lightdb_stream_set_json_sync(client, "sensor", sbuf, strlen(sbuf), 1);

    if (err) {
        LOG_WRN("Failed to push temperature: %d", err);
        return;
    }

    LOG_DBG("Temperature successfully pushed");
}

static void temperature_push_float_async(const struct sensor_value *temp) {
    int err;

    err = golioth_lightdb_stream_set_float_async(client,
                                                 "sensor/temp",
                                                 sensor_value_to_double(temp),
                                                 temperature_push_handler,
                                                 NULL);

    if (err) {
        LOG_WRN("Failed to push temperature: %d", err);
        return;
    }
}

int main(void) {
    struct sensor_value temp;
    int err;

    LOG_DBG("Start LightDB Stream sample");

    IF_ENABLED(CONFIG_GOLIOTH_SAMPLE_COMMON, (net_connect();))

    /* Note: In production, you would provision unique credentials onto each
     * device. For simplicity, we provide a utility to hardcode credentials as
     * kconfig options in the samples.
     */
    const struct golioth_client_config *client_config = golioth_sample_credentials_get();

    client = golioth_client_create(client_config);
    golioth_client_register_event_callback(client, on_client_event, NULL);

    k_sem_take(&connected, K_FOREVER);

    while (true) {
        /* Synchronous using JSON object */
        err = get_temperature(&temp);
        if (err) {
            k_sleep(K_SECONDS(1));
            continue;
        }

        LOG_DBG("Sending temperature %d.%06d", temp.val1, abs(temp.val2));

        temperature_push_sync(&temp);

        k_sleep(K_SECONDS(5));

        /* Callback-based using JSON object */
        err = get_temperature(&temp);
        if (err) {
            k_sleep(K_SECONDS(1));
            continue;
        }

        LOG_DBG("Sending temperature %d.%06d", temp.val1, abs(temp.val2));

        temperature_push_async(&temp);
        k_sleep(K_SECONDS(5));

        /* Callback-based using float */
        err = get_temperature(&temp);
        if (err) {
            k_sleep(K_SECONDS(1));
            continue;
        }

        LOG_DBG("Sending temperature %d.%06d", temp.val1, abs(temp.val2));

        temperature_push_float_async(&temp);
        k_sleep(K_SECONDS(5));
    }

    return 0;
}
