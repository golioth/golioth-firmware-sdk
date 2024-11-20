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

static void test_lightdb_desired_reported_sync(struct golioth_client *client)
{
    enum golioth_status status;
    bool val_bool;
    int32_t val_int;
    float val_float;
    uint8_t val_buf[256];
    size_t val_buf_len;

    status = golioth_lightdb_get_bool_sync(client, "hil/lightdb/desired/true", &val_bool, TIMEOUT);
    if (status == GOLIOTH_OK)
    {
        golioth_lightdb_set_bool_sync(client, "hil/lightdb/reported/sync/true", val_bool, TIMEOUT);
    }

    golioth_lightdb_get_bool_sync(client, "hil/lightdb/desired/false", &val_bool, TIMEOUT);
    if (status == GOLIOTH_OK)
    {
        golioth_lightdb_set_bool_sync(client, "hil/lightdb/reported/sync/false", val_bool, TIMEOUT);
    }

    golioth_lightdb_get_int_sync(client, "hil/lightdb/desired/int", &val_int, TIMEOUT);
    if (status == GOLIOTH_OK)
    {
        golioth_lightdb_set_int_sync(client, "hil/lightdb/reported/sync/int", val_int, TIMEOUT);
    }

    golioth_lightdb_get_float_sync(client, "hil/lightdb/desired/float", &val_float, TIMEOUT);
    if (status == GOLIOTH_OK)
    {
        golioth_lightdb_set_float_sync(client,
                                       "hil/lightdb/reported/sync/float",
                                       val_float,
                                       TIMEOUT);
    }

    golioth_lightdb_get_string_sync(client,
                                    "hil/lightdb/desired/str",
                                    (char *) val_buf,
                                    sizeof(val_buf),
                                    TIMEOUT);
    if (status == GOLIOTH_OK)
    {
        golioth_lightdb_set_string_sync(client,
                                        "hil/lightdb/reported/sync/str",
                                        (char *) val_buf,
                                        strlen((char *) val_buf),
                                        TIMEOUT);
    }

    val_buf_len = sizeof(val_buf);
    golioth_lightdb_get_sync(client,
                             "hil/lightdb/desired/obj",
                             GOLIOTH_CONTENT_TYPE_JSON,
                             val_buf,
                             &val_buf_len,
                             TIMEOUT);
    if (status == GOLIOTH_OK)
    {
        golioth_lightdb_set_sync(client,
                                 "hil/lightdb/reported/sync/obj",
                                 GOLIOTH_CONTENT_TYPE_JSON,
                                 val_buf,
                                 val_buf_len,
                                 TIMEOUT);
    }

    status =
        golioth_lightdb_get_bool_sync(client, "hil/lightdb/desired///invalid", &val_bool, TIMEOUT);
    golioth_lightdb_set_string_sync(client,
                                    "hil/lightdb/reported/sync/invalid",
                                    golioth_status_to_str(status),
                                    strlen(golioth_status_to_str(status)),
                                    TIMEOUT);

    status =
        golioth_lightdb_get_bool_sync(client, "hil/lightdb/desired/nothing", &val_bool, TIMEOUT);
    golioth_lightdb_set_string_sync(client,
                                    "hil/lightdb/reported/sync/nothing",
                                    golioth_status_to_str(status),
                                    strlen(golioth_status_to_str(status)),
                                    TIMEOUT);
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

static void test_lightdb_desired_reported_async(struct golioth_client *client)
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

    status = golioth_lightdb_get_async(client,
                                       "hil/lightdb/desired/true",
                                       GOLIOTH_CONTENT_TYPE_JSON,
                                       get_cb,
                                       &(struct lightdb_get_rsp){
                                           .type = LIGHTDB_GET_TYPE_BOOL,
                                           .b = &val_bool,
                                           .sem = sem,
                                       });
    if (status == GOLIOTH_OK)
    {
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
        golioth_lightdb_set_bool_async(client,
                                       "hil/lightdb/reported/async/true",
                                       val_bool,
                                       set_cb,
                                       sem);
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
    }

    status = golioth_lightdb_get_async(client,
                                       "hil/lightdb/desired/false",
                                       GOLIOTH_CONTENT_TYPE_JSON,
                                       get_cb,
                                       &(struct lightdb_get_rsp){
                                           .type = LIGHTDB_GET_TYPE_BOOL,
                                           .b = &val_bool,
                                           .sem = sem,
                                       });
    if (status == GOLIOTH_OK)
    {
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
        golioth_lightdb_set_bool_async(client,
                                       "hil/lightdb/reported/async/false",
                                       val_bool,
                                       set_cb,
                                       sem);
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
    }

    status = golioth_lightdb_get_async(client,
                                       "hil/lightdb/desired/int",
                                       GOLIOTH_CONTENT_TYPE_JSON,
                                       get_cb,
                                       &(struct lightdb_get_rsp){
                                           .type = LIGHTDB_GET_TYPE_INT32,
                                           .i32 = &val_int32,
                                           .sem = sem,
                                       });
    if (status == GOLIOTH_OK)
    {
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
        golioth_lightdb_set_int_async(client,
                                      "hil/lightdb/reported/async/int",
                                      val_int32,
                                      set_cb,
                                      sem);
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
    }

    status = golioth_lightdb_get_async(client,
                                       "hil/lightdb/desired/float",
                                       GOLIOTH_CONTENT_TYPE_JSON,
                                       get_cb,
                                       &(struct lightdb_get_rsp){
                                           .type = LIGHTDB_GET_TYPE_FLOAT,
                                           .f = &val_float,
                                           .sem = sem,
                                       });
    if (status == GOLIOTH_OK)
    {
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
        golioth_lightdb_set_float_async(client,
                                        "hil/lightdb/reported/async/float",
                                        val_float,
                                        set_cb,
                                        sem);
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
    }

    val_buf_len = sizeof(val_buf);
    status = golioth_lightdb_get_async(client,
                                       "hil/lightdb/desired/str",
                                       GOLIOTH_CONTENT_TYPE_JSON,
                                       get_cb,
                                       &(struct lightdb_get_rsp){
                                           .type = LIGHTDB_GET_TYPE_STRING,
                                           .buf = val_buf,
                                           .buf_size = &val_buf_len,
                                           .sem = sem,
                                       });
    if (status == GOLIOTH_OK)
    {
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
        val_buf[val_buf_len] = 0;
        golioth_lightdb_set_string_async(client,
                                         "hil/lightdb/reported/async/str",
                                         (char *) val_buf,
                                         val_buf_len,
                                         set_cb,
                                         sem);
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
    }

    val_buf_len = sizeof(val_buf);
    status = golioth_lightdb_get_async(client,
                                       "hil/lightdb/desired/obj",
                                       GOLIOTH_CONTENT_TYPE_JSON,
                                       get_cb,
                                       &(struct lightdb_get_rsp){
                                           .type = LIGHTDB_GET_TYPE_RAW,
                                           .buf = val_buf,
                                           .buf_size = &val_buf_len,
                                           .sem = sem,
                                       });
    if (status == GOLIOTH_OK)
    {
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
        golioth_lightdb_set_async(client,
                                  "hil/lightdb/reported/async/obj",
                                  GOLIOTH_CONTENT_TYPE_JSON,
                                  val_buf,
                                  val_buf_len,
                                  set_cb,
                                  sem);
        golioth_sys_sem_take(sem, TIMEOUT * 1000);
    }

    golioth_sys_sem_destroy(sem);
}

static void test_lightdb_delete_sync(struct golioth_client *client)
{
    golioth_lightdb_delete_sync(client, "hil/lightdb/to_delete/sync/true", TIMEOUT);
    golioth_lightdb_delete_sync(client, "hil/lightdb/to_delete/sync/false", TIMEOUT);
    golioth_lightdb_delete_sync(client, "hil/lightdb/to_delete/sync/int", TIMEOUT);
    golioth_lightdb_delete_sync(client, "hil/lightdb/to_delete/sync/str", TIMEOUT);
    golioth_lightdb_delete_sync(client, "hil/lightdb/to_delete/sync/obj", TIMEOUT);
}

static void test_lightdb_delete_async(struct golioth_client *client)
{
    golioth_sys_sem_t sem;

    sem = golioth_sys_sem_create(1, 0);
    if (!sem)
    {
        return;
    }

    golioth_lightdb_delete_async(client, "hil/lightdb/to_delete/async/true", set_cb, sem);
    golioth_sys_sem_take(sem, TIMEOUT * 1000);
    golioth_lightdb_delete_async(client, "hil/lightdb/to_delete/async/false", set_cb, sem);
    golioth_sys_sem_take(sem, TIMEOUT * 1000);
    golioth_lightdb_delete_async(client, "hil/lightdb/to_delete/async/int", set_cb, sem);
    golioth_sys_sem_take(sem, TIMEOUT * 1000);
    golioth_lightdb_delete_async(client, "hil/lightdb/to_delete/async/str", set_cb, sem);
    golioth_sys_sem_take(sem, TIMEOUT * 1000);
    golioth_lightdb_delete_async(client, "hil/lightdb/to_delete/async/obj", set_cb, sem);
    golioth_sys_sem_take(sem, TIMEOUT * 1000);

    golioth_sys_sem_destroy(sem);
}

static void test_lightdb_invalid(struct golioth_client *client)
{
    enum golioth_status status;

    status = golioth_lightdb_set_sync(client,
                                      "hil/lightdb/invalid/dot",
                                      GOLIOTH_CONTENT_TYPE_JSON,
                                      (const uint8_t *) ".",
                                      1,
                                      TIMEOUT);

    golioth_lightdb_set_string_sync(client,
                                    "hil/lightdb/invalid/sync/set_dot",
                                    golioth_status_to_str(status),
                                    strlen(golioth_status_to_str(status)),
                                    TIMEOUT);
}

static void test_lightdb_observe_setup(struct golioth_client *client)
{
    golioth_lightdb_observe_async(client,
                                  "hil/lightdb/observed/int",
                                  GOLIOTH_CONTENT_TYPE_JSON,
                                  int_cb,
                                  NULL);

    golioth_lightdb_observe_async(client,
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
            golioth_lightdb_set_sync(client,
                                     event_path,
                                     GOLIOTH_CONTENT_TYPE_JSON,
                                     observed_data[events_count].observed_payload,
                                     observed_data[events_count].observed_payload_size,
                                     TIMEOUT);
        }
        else
        {
            /* Use CBOR */
            golioth_lightdb_set_sync(client,
                                     event_path,
                                     GOLIOTH_CONTENT_TYPE_CBOR,
                                     observed_data[events_count].observed_payload,
                                     observed_data[events_count].observed_payload_size,
                                     TIMEOUT);
        }

        golioth_lightdb_set_int_sync(client,
                                     "hil/lightdb/observed/events/count",
                                     events_count,
                                     TIMEOUT);
    }
}

void hil_test_entry(const struct golioth_client_config *config)
{
    struct golioth_client *client = golioth_client_create(config);
    enum golioth_status status;
    bool val_bool;

    observed_sem = golioth_sys_sem_create(10, 0);

    while (true)
    {
        status =
            golioth_lightdb_get_bool_sync(client, "hil/lightdb/desired/ready", &val_bool, TIMEOUT);
        if (status == GOLIOTH_OK && val_bool == true)
        {
            GLTH_LOGI(TAG, "LightDB State desired data is ready");
            break;
        }

        golioth_sys_msleep(1000);
    }

    test_lightdb_observe_setup(client);

    test_lightdb_desired_reported_sync(client);
    test_lightdb_desired_reported_async(client);
    test_lightdb_delete_sync(client);
    test_lightdb_delete_async(client);
    test_lightdb_invalid(client);

    GLTH_LOGI(TAG, "LightDB State reported data is ready");

    test_lightdb_observe(client);

    while (1)
    {
        golioth_sys_msleep(5000);
    }
}
