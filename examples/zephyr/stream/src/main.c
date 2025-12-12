/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_stream, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/stream.h>
#include <samples/common/sample_credentials.h>
#include <samples/common/net_connect.h>
#include <stdlib.h>
#include <string.h>
#include <zcbor_encode.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

#define SYNC_TIMEOUT_S 2

struct golioth_client *client;
static K_SEM_DEFINE(connected, 0, 1);

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected)
    {
        k_sem_give(&connected);
    }
    LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}

#if DT_NODE_HAS_STATUS(DT_ALIAS(temp0), okay)

static int get_temperature(struct sensor_value *val)
{
    const struct device *dev = DEVICE_DT_GET(DT_ALIAS(temp0));
    static const enum sensor_channel temp_channels[] = {
        SENSOR_CHAN_AMBIENT_TEMP,
        SENSOR_CHAN_DIE_TEMP,
    };
    int i;
    int err;

    err = sensor_sample_fetch(dev);
    if (err)
    {
        LOG_ERR("Failed to fetch temperature sensor: %d", err);
        return err;
    }

    for (i = 0; i < ARRAY_SIZE(temp_channels); i++)
    {
        err = sensor_channel_get(dev, temp_channels[i], val);
        if (err == -ENOTSUP)
        {
            /* try next channel */
            continue;
        }
        else if (err)
        {
            LOG_ERR("Failed to get temperature: %d", err);
            return err;
        }
        break;
    }

    return err;
}

#else

static int get_temperature(struct sensor_value *val)
{
    static int counter = 0;

    /* generate a temperature from 20 deg to 30 deg, with 0.5 deg step */

    val->val1 = 20 + counter / 2;
    val->val2 = counter % 2 == 1 ? 500000 : 0;

    counter = (counter + 1) % 20;

    return 0;
}

#endif

static void temperature_async_push_handler(struct golioth_client *client,
                                           enum golioth_status status,
                                           const struct golioth_coap_rsp_code *coap_rsp_code,
                                           const char *path,
                                           void *arg)
{
    if (status != GOLIOTH_OK)
    {
        LOG_WRN("Failed to push temperature: %d (%s)", status, golioth_status_to_str(status));
        return;
    }

    LOG_DBG("Temperature successfully pushed");

    return;
}

static void temperature_push_cbor(const struct sensor_value *temp, bool async)
{
    uint8_t buf[15];
    int err;

    /* Create zcbor state variable for encoding */
    ZCBOR_STATE_E(zse, 1, buf, sizeof(buf), 1);

    /* Encode the CBOR map header */
    bool ok = zcbor_map_start_encode(zse, 1);
    if (!ok)
    {
        LOG_ERR("Failed to start encoding CBOR map");
        return;
    }

    /* Encode the map key "temp" into buf as a text string */
    ok = zcbor_tstr_put_lit(zse, "temp");
    if (!ok)
    {
        LOG_ERR("CBOR: Failed to encode temp name");
        return;
    }

    /* Encode the temperature value into buf as a float */
    ok = zcbor_float32_put(zse, sensor_value_to_float(temp));
    if (!ok)
    {
        LOG_ERR("CBOR: failed to encode temp value");
        return;
    }

    /* Close the CBOR map */
    ok = zcbor_map_end_encode(zse, 1);
    if (!ok)
    {
        LOG_ERR("Failed to close CBOR map");
        return;
    }

    size_t payload_size = (intptr_t) zse->payload - (intptr_t) buf;

    /* Data is sent to Pipelines on the "/data" path */
    if (async)
    {
        err = golioth_stream_set_async(client,
                                       "data",
                                       GOLIOTH_CONTENT_TYPE_CBOR,
                                       buf,
                                       payload_size,
                                       temperature_async_push_handler,
                                       NULL);
    }
    else
    {
        err = golioth_stream_set_sync(client,
                                      "data",
                                      GOLIOTH_CONTENT_TYPE_CBOR,
                                      buf,
                                      payload_size,
                                      SYNC_TIMEOUT_S);
    }

    if (err)
    {
        LOG_WRN("Failed to push temperature: %d (%s)", err, golioth_status_to_str(err));
        return;
    }

    if (!async)
    {
        LOG_DBG("Temperature successfully pushed");
    }
}

int main(void)
{
    struct sensor_value temp;
    int err;

    LOG_DBG("Start Golioth stream sample");

    net_connect();

    /* Note: In production, credentials should be saved in secure storage. For
     * simplicity, we provide a utility that stores credentials as plaintext
     * settings.
     */
    const struct golioth_client_config *client_config = golioth_sample_credentials_get();

    client = golioth_client_create(client_config);
    golioth_client_register_event_callback(client, on_client_event, NULL);

    k_sem_take(&connected, K_FOREVER);

    while (true)
    {
        /* Synchronous using CBOR map */
        err = get_temperature(&temp);
        if (err)
        {
            k_sleep(K_SECONDS(1));
            continue;
        }

        LOG_INF("Sending temperature %d.%06d (sync)", temp.val1, abs(temp.val2));

        temperature_push_cbor(&temp, false);

        k_sleep(K_SECONDS(5));

        /* Callback-based using CBOR map */
        err = get_temperature(&temp);
        if (err)
        {
            k_sleep(K_SECONDS(1));
            continue;
        }

        LOG_INF("Sending temperature %d.%06d (async)", temp.val1, abs(temp.val2));

        temperature_push_cbor(&temp, true);

        k_sleep(K_SECONDS(5));
    }

    return 0;
}
