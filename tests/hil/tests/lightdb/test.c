/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <golioth/client.h>
#include <golioth/golioth_debug.h>
#include <golioth/golioth_sys.h>
#include <golioth/lightdb_state.h>
#include <golioth/payload_utils.h>
#include <golioth/zcbor_utils.h>

LOG_TAG_DEFINE(test_lightdb);

#define TIMEOUT 30

#define MAX_OBSERVED_EVENTS 6
static unsigned int observed_cbor_events_count;
static unsigned int observed_json_events_count;
static golioth_sys_sem_t observed_sem;

struct obs_data
{
    enum golioth_content_type type;
    size_t observed_payload_size;
    uint8_t observed_payload[256];
};

static struct obs_data observed_data[MAX_OBSERVED_EVENTS];

static void int_cb(struct golioth_client *client,
                   enum golioth_status status,
                   const struct golioth_coap_rsp_code *coap_rsp_code,
                   const char *path,
                   const uint8_t *payload,
                   size_t payload_size,
                   void *arg)
{
    if (status != GOLIOTH_OK)
    {
        GLTH_LOGE(TAG, "Failed to received observed int: %d", status);
    }

    int idx = observed_cbor_events_count + observed_json_events_count;
    memcpy(observed_data[idx].observed_payload,
           payload,
           MIN(payload_size, sizeof(observed_data[idx].observed_payload)));
    observed_data[idx].observed_payload_size = payload_size;
    observed_data[idx].type = GOLIOTH_CONTENT_TYPE_JSON;
    observed_json_events_count++;

    golioth_sys_sem_give(observed_sem);
}

static void int_cbor_cb(struct golioth_client *client,
                        enum golioth_status status,
                        const struct golioth_coap_rsp_code *coap_rsp_code,
                        const char *path,
                        const uint8_t *payload,
                        size_t payload_size,
                        void *arg)
{
    if (status != GOLIOTH_OK)
    {
        GLTH_LOGE(TAG, "Failed to received observed int: %d", status);
    }

    int idx = observed_cbor_events_count + observed_json_events_count;
    memcpy(observed_data[idx].observed_payload,
           payload,
           MIN(payload_size, sizeof(observed_data[idx].observed_payload)));
    observed_data[idx].observed_payload_size = payload_size;
    observed_data[idx].type = GOLIOTH_CONTENT_TYPE_CBOR;
    observed_cbor_events_count++;

    golioth_sys_sem_give(observed_sem);
}

enum lightdb_get_type
{
    LIGHTDB_GET_TYPE_BOOL,
    LIGHTDB_GET_TYPE_INT32,
    LIGHTDB_GET_TYPE_FLOAT,
    LIGHTDB_GET_TYPE_STRING,
    LIGHTDB_GET_TYPE_RAW,
};

struct lightdb_get_rsp
{
    enum lightdb_get_type type;
    union
    {
        bool *b;
        int32_t *i32;
        float *f;
        struct
        {
            uint8_t *buf;
            size_t *buf_size;
        };
    };
    golioth_sys_sem_t sem;
};

static void get_cb(struct golioth_client *client,
                   enum golioth_status status,
                   const struct golioth_coap_rsp_code *coap_rsp_code,
                   const char *path,
                   const uint8_t *payload,
                   size_t payload_size,
                   void *arg)
{
    struct lightdb_get_rsp *rsp = arg;

    switch (rsp->type)
    {
        case LIGHTDB_GET_TYPE_BOOL:
            *rsp->b = golioth_payload_as_bool(payload, payload_size);
            break;
        case LIGHTDB_GET_TYPE_INT32:
            *rsp->i32 = golioth_payload_as_int(payload, payload_size);
            break;
        case LIGHTDB_GET_TYPE_FLOAT:
            *rsp->f = golioth_payload_as_float(payload, payload_size);
            break;
        case LIGHTDB_GET_TYPE_STRING:
            /* Get rid of double quotes */
            payload_size = MIN(payload_size - 2, *rsp->buf_size);
            memcpy(rsp->buf, payload + 1, payload_size);
            *rsp->buf_size = payload_size;
            break;
        case LIGHTDB_GET_TYPE_RAW:
            payload_size = MIN(payload_size, *rsp->buf_size);
            memcpy(rsp->buf, payload, payload_size);
            *rsp->buf_size = payload_size;
            break;
    }

    golioth_sys_sem_give(rsp->sem);
}

static void set_cb(struct golioth_client *client,
                   enum golioth_status status,
                   const struct golioth_coap_rsp_code *coap_rsp_code,
                   const char *path,
                   void *arg)
{
    golioth_sys_sem_t sem = arg;

    golioth_sys_sem_give(sem);
}

static void test_lightdb_desired_reported(struct golioth_client *client)
{
    enum golioth_status status;
    bool val_bool;
    int32_t val_int32;
    float val_float;
    golioth_sys_sem_t sem;
    uint8_t val_buf[256];
    size_t val_buf_len;

    sem = golioth_sys_sem_create(1, 0);
    if (!sem)
    {
        return;
    }

    status = golioth_lightdb_get(client,
                                 "hil/lightdb/desired/true",
                                 GOLIOTH_CONTENT_TYPE_JSON,
                                 get_cb,
                                 &(struct lightdb_get_rsp) {
                                     .type = LIGHTDB_GET_TYPE_BOOL,
                                     .b = &val_bool,
                                     .sem = sem,
                                 });
    if (status == GOLIOTH_OK)
    {
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
        golioth_lightdb_set_bool(client, "hil/lightdb/reported/true", val_bool, set_cb, sem);
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
    }

    status = golioth_lightdb_get(client,
                                 "hil/lightdb/desired/false",
                                 GOLIOTH_CONTENT_TYPE_JSON,
                                 get_cb,
                                 &(struct lightdb_get_rsp) {
                                     .type = LIGHTDB_GET_TYPE_BOOL,
                                     .b = &val_bool,
                                     .sem = sem,
                                 });
    if (status == GOLIOTH_OK)
    {
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
        golioth_lightdb_set_bool(client, "hil/lightdb/reported/false", val_bool, set_cb, sem);
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
    }

    status = golioth_lightdb_get(client,
                                 "hil/lightdb/desired/int",
                                 GOLIOTH_CONTENT_TYPE_JSON,
                                 get_cb,
                                 &(struct lightdb_get_rsp) {
                                     .type = LIGHTDB_GET_TYPE_INT32,
                                     .i32 = &val_int32,
                                     .sem = sem,
                                 });
    if (status == GOLIOTH_OK)
    {
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
        golioth_lightdb_set_int(client, "hil/lightdb/reported/int", val_int32, set_cb, sem);
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
    }

    status = golioth_lightdb_get(client,
                                 "hil/lightdb/desired/float",
                                 GOLIOTH_CONTENT_TYPE_JSON,
                                 get_cb,
                                 &(struct lightdb_get_rsp) {
                                     .type = LIGHTDB_GET_TYPE_FLOAT,
                                     .f = &val_float,
                                     .sem = sem,
                                 });
    if (status == GOLIOTH_OK)
    {
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
        golioth_lightdb_set_float(client, "hil/lightdb/reported/float", val_float, set_cb, sem);
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
    }

    val_buf_len = sizeof(val_buf);
    status = golioth_lightdb_get(client,
                                 "hil/lightdb/desired/str",
                                 GOLIOTH_CONTENT_TYPE_JSON,
                                 get_cb,
                                 &(struct lightdb_get_rsp) {
                                     .type = LIGHTDB_GET_TYPE_STRING,
                                     .buf = val_buf,
                                     .buf_size = &val_buf_len,
                                     .sem = sem,
                                 });
    if (status == GOLIOTH_OK)
    {
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
        val_buf[val_buf_len] = 0;
        golioth_lightdb_set_string(client,
                                   "hil/lightdb/reported/str",
                                   (char *) val_buf,
                                   val_buf_len,
                                   set_cb,
                                   sem);
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
    }

    val_buf_len = sizeof(val_buf);
    status = golioth_lightdb_get(client,
                                 "hil/lightdb/desired/obj",
                                 GOLIOTH_CONTENT_TYPE_JSON,
                                 get_cb,
                                 &(struct lightdb_get_rsp) {
                                     .type = LIGHTDB_GET_TYPE_RAW,
                                     .buf = val_buf,
                                     .buf_size = &val_buf_len,
                                     .sem = sem,
                                 });
    if (status == GOLIOTH_OK)
    {
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
        golioth_lightdb_set(client,
                            "hil/lightdb/reported/obj",
                            GOLIOTH_CONTENT_TYPE_JSON,
                            val_buf,
                            val_buf_len,
                            set_cb,
                            sem);
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
    }

    golioth_sys_sem_destroy(sem);
}

static void test_lightdb_delete(struct golioth_client *client)
{
    golioth_sys_sem_t sem;

    sem = golioth_sys_sem_create(1, 0);
    if (!sem)
    {
        return;
    }

    golioth_lightdb_delete(client, "hil/lightdb/to_delete/true", set_cb, sem);
    golioth_sys_sem_take(sem, TIMEOUT * 1000);
    golioth_lightdb_delete(client, "hil/lightdb/to_delete/false", set_cb, sem);
    golioth_sys_sem_take(sem, TIMEOUT * 1000);
    golioth_lightdb_delete(client, "hil/lightdb/to_delete/int", set_cb, sem);
    golioth_sys_sem_take(sem, TIMEOUT * 1000);
    golioth_lightdb_delete(client, "hil/lightdb/to_delete/str", set_cb, sem);
    golioth_sys_sem_take(sem, TIMEOUT * 1000);
    golioth_lightdb_delete(client, "hil/lightdb/to_delete/obj", set_cb, sem);
    golioth_sys_sem_take(sem, TIMEOUT * 1000);

    golioth_sys_sem_destroy(sem);
}

static void test_lightdb_observe_setup(struct golioth_client *client)
{
    golioth_lightdb_observe(client,
                            "hil/lightdb/observed/int",
                            GOLIOTH_CONTENT_TYPE_JSON,
                            int_cb,
                            NULL);

    golioth_lightdb_observe(client,
                            "hil/lightdb/observed/cbor/int",
                            GOLIOTH_CONTENT_TYPE_CBOR,
                            int_cbor_cb,
                            NULL);
}

static void test_lightdb_observe(struct golioth_client *client)
{

    for (int events_count = 0; events_count < MAX_OBSERVED_EVENTS; events_count++)
    {
        char event_path[64];

        golioth_sys_sem_take(observed_sem, 30000);

        sprintf(event_path, "hil/lightdb/observed/events/%u", events_count);

        if (observed_data[events_count].type == GOLIOTH_CONTENT_TYPE_JSON)
        {
            /* Use JSON */
            golioth_lightdb_set(client,
                                event_path,
                                GOLIOTH_CONTENT_TYPE_JSON,
                                observed_data[events_count].observed_payload,
                                observed_data[events_count].observed_payload_size,
                                NULL,
                                NULL);
        }
        else
        {
            /* Use CBOR */
            golioth_lightdb_set(client,
                                event_path,
                                GOLIOTH_CONTENT_TYPE_CBOR,
                                observed_data[events_count].observed_payload,
                                observed_data[events_count].observed_payload_size,
                                NULL,
                                NULL);
        }

        golioth_lightdb_set_int(client,
                                "hil/lightdb/observed/events/count",
                                events_count,
                                NULL,
                                NULL);
    }
}

static void on_ready(struct golioth_client *client,
                     enum golioth_status status,
                     const struct golioth_coap_rsp_code *coap_rsp_code,
                     const char *path,
                     const uint8_t *payload,
                     size_t payload_size,
                     void *arg)
{
    if (golioth_payload_as_bool(payload, payload_size))
    {
        golioth_sys_sem_t *ready_sem = arg;
        golioth_sys_sem_give(*ready_sem);
    }
}

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    golioth_sys_sem_t *connected_sem = arg;

    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected)
    {
        golioth_sys_sem_give(*connected_sem);
    }
}

void hil_test_entry(const struct golioth_client_config *config)
{
    golioth_sys_sem_t connected_sem = golioth_sys_sem_create(1, 0);

    struct golioth_client *client = golioth_client_create(config);

    golioth_client_register_event_callback(client, on_client_event, &connected_sem);
    golioth_sys_sem_take(connected_sem, GOLIOTH_SYS_WAIT_FOREVER);

    observed_sem = golioth_sys_sem_create(10, 0);

    golioth_sys_sem_t ready_sem = golioth_sys_sem_create(1, 0);

    golioth_lightdb_observe(client,
                            "hil/lightdb/desired/ready",
                            GOLIOTH_CONTENT_TYPE_JSON,
                            on_ready,
                            &ready_sem);

    golioth_sys_sem_take(ready_sem, GOLIOTH_SYS_WAIT_FOREVER);

    test_lightdb_observe_setup(client);

    test_lightdb_desired_reported(client);
    test_lightdb_delete(client);

    GLTH_LOGI(TAG, "LightDB State reported data is ready");

    test_lightdb_observe(client);

    while (1)
    {
        golioth_sys_msleep(5000);
    }
}
