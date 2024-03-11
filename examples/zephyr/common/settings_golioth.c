/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_sample_settings, CONFIG_GOLIOTH_DEBUG_DEFAULT_LOG_LEVEL);

#include <errno.h>
#include <golioth/client.h>
#include <zephyr/init.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/settings/settings.h>

/*
 * TLS credentials subsystem just remembers pointers to memory areas where
 * credentials are stored. This means that we need to allocate memory for
 * credentials ourselves.
 */
static uint8_t golioth_dtls_psk[CONFIG_GOLIOTH_SAMPLE_PSK_MAX_LEN];
static uint8_t golioth_dtls_psk_id[CONFIG_GOLIOTH_SAMPLE_PSK_ID_MAX_LEN];

static struct golioth_client_config client_config = {
    .credentials =
        {
            .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
            .psk =
                {
                    .psk_id = golioth_dtls_psk_id,
                    .psk_id_len = 0,
                    .psk = golioth_dtls_psk,
                    .psk_len = 0,
                },
        },
};

static void set_runtime_tls_credential(sec_tag_t tag,
                                       enum tls_credential_type type,
                                       const void *cred,
                                       size_t credlen)
{
    int err;

    err = tls_credential_delete(tag, type);
    if ((err) && (err != -ENOENT))
    {
        LOG_ERR("Failed to delete TLS credential; Tag: %d, Error: %d", tag, err);
    }

    err = tls_credential_add(tag, type, cred, credlen);
    if (err)
    {
        LOG_ERR("Failed to add TLS credential; Tag: %d, Error: %d", tag, err);
        return;
    }
}

static int golioth_settings_get(const char *name, char *dst, int val_len_max)
{
    uint8_t *val;
    size_t val_len;

    if (!strcmp(name, "psk"))
    {
        val = golioth_dtls_psk;
        val_len = strlen(golioth_dtls_psk);
    }
    else if (!strcmp(name, "psk-id"))
    {
        val = golioth_dtls_psk_id;
        val_len = strlen(golioth_dtls_psk_id);
    }
    else
    {
        LOG_WRN("Unsupported key '%s'", name);
        return -ENOENT;
    }

    if (val_len > val_len_max)
    {
        LOG_ERR("Not enough space (%zu %d)", val_len, val_len_max);
        return -ENOMEM;
    }

    memcpy(dst, val, val_len);

    return val_len;
}

static int golioth_settings_set(const char *name,
                                size_t len_rd,
                                settings_read_cb read_cb,
                                void *cb_arg)
{
    uint8_t *buffer;
    size_t buffer_len;
    size_t *ret_len;
    ssize_t ret;

    if (!strcmp(name, "psk"))
    {
        if (len_rd > CONFIG_GOLIOTH_SAMPLE_PSK_MAX_LEN)
        {
            LOG_ERR("PSK length is %d but CONFIG_GOLIOTH_SAMPLE_PSK_MAX_LEN is %d.",
                    len_rd,
                    CONFIG_GOLIOTH_SAMPLE_PSK_MAX_LEN);
            return -E2BIG;
        }
        buffer = golioth_dtls_psk;
        buffer_len = sizeof(golioth_dtls_psk);
        ret_len = &client_config.credentials.psk.psk_len;

        set_runtime_tls_credential(CONFIG_GOLIOTH_COAP_CLIENT_CREDENTIALS_TAG,
                                   TLS_CREDENTIAL_PSK,
                                   buffer,
                                   len_rd);
    }
    else if (!strcmp(name, "psk-id"))
    {
        if (len_rd > CONFIG_GOLIOTH_SAMPLE_PSK_ID_MAX_LEN)
        {
            LOG_ERR("PSK-ID length is %d but CONFIG_GOLIOTH_SAMPLE_PSK_ID_MAX_LEN is %d.",
                    len_rd,
                    CONFIG_GOLIOTH_SAMPLE_PSK_ID_MAX_LEN);
            return -E2BIG;
        }
        buffer = golioth_dtls_psk_id;
        buffer_len = sizeof(golioth_dtls_psk_id);
        ret_len = &client_config.credentials.psk.psk_id_len;

        set_runtime_tls_credential(CONFIG_GOLIOTH_COAP_CLIENT_CREDENTIALS_TAG,
                                   TLS_CREDENTIAL_PSK_ID,
                                   buffer,
                                   len_rd);
    }
    else
    {
        LOG_ERR("Unsupported key '%s'", name);
        return -ENOTSUP;
    }

    ret = read_cb(cb_arg, buffer, buffer_len);
    if (ret < 0)
    {
        LOG_ERR("Failed to read value: %d", (int) ret);
        return ret;
    }

    *ret_len = ret;

    return 0;
}

static int golioth_settings_init(void)
{
    int err = settings_subsys_init();

    if (err)
    {
        LOG_ERR("Failed to initialize settings subsystem: %d", err);
        return err;
    }

    return 0;
}

SYS_INIT(golioth_settings_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

SETTINGS_STATIC_HANDLER_DEFINE(golioth,
                               "golioth",
                               IS_ENABLED(CONFIG_SETTINGS_RUNTIME) ? golioth_settings_get : NULL,
                               golioth_settings_set,
                               NULL,
                               NULL);

const struct golioth_client_config *golioth_sample_credentials_get(void)
{
    return &client_config;
}
