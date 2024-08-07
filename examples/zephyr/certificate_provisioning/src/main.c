/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cert_provisioning, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <samples/common/net_connect.h>
#include <zephyr/kernel.h>
#include <zephyr/net/coap.h>
#include <zephyr/device.h>
#include <zephyr/fs/fs.h>

#define CLIENT_CERTIFICATE_PATH "/lfs1/credentials/client_cert.der"
#define PRIVATE_KEY_PATH "/lfs1/credentials/private_key.der"

static const uint8_t tls_ca_crt[] = {
#include "golioth-systemclient-ca_crt.inc"
};

static const uint8_t tls_secondary_ca_crt[] = {
#if CONFIG_GOLIOTH_SECONDARY_CA_CRT
#include "golioth-systemclient-secondary_ca_crt.inc"
#endif
};

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

static int load_credential_from_fs(const char *path, uint8_t **buf_p, size_t *buf_len)
{
    struct fs_file_t file;
    struct fs_dirent dirent;

    fs_file_t_init(&file);

    int err = fs_stat(path, &dirent);

    if (err < 0)
    {
        LOG_WRN("Could not stat %s, err: %d", path, err);
        goto finish;
    }
    if (dirent.type != FS_DIR_ENTRY_FILE)
    {
        LOG_ERR("%s is not a file", path);
        err = -EISDIR;
        goto finish;
    }
    if (dirent.size == 0)
    {
        LOG_ERR("%s is an empty file", path);
        err = -EINVAL;
        goto finish;
    }


    err = fs_open(&file, path, FS_O_READ);

    if (err < 0)
    {
        LOG_ERR("Could not open %s", path);
        goto finish;
    }

    /* NOTE: *buf_p is used directly by the TLS Credentials library, and so must remain
     * allocated for the life of the program.
     */

    free(*buf_p);
    *buf_p = malloc(dirent.size);

    if (*buf_p == NULL)
    {
        LOG_ERR("Could not allocate space to read credential");
        err = -ENOMEM;
        goto finish_with_file;
    }

    err = fs_read(&file, *buf_p, dirent.size);

    if (err < 0)
    {
        LOG_ERR("Could not read %s, err: %d", path, err);
        free(*buf_p);
        goto finish_with_file;
    }

    LOG_INF("Read %d bytes from %s", dirent.size, path);

    /* Set the size of the allocated buffer */
    *buf_len = dirent.size;

finish_with_file:
    fs_close(&file);

finish:
    return err;
}

int main(void)
{
    int counter = 0;

    LOG_DBG("Start certificate provisioning sample");

    net_connect();

    uint8_t *tls_client_crt = NULL;
    uint8_t *tls_client_key = NULL;
    size_t tls_client_crt_len, tls_client_key_len;

    while (1)
    {
        int ret =
            load_credential_from_fs(CLIENT_CERTIFICATE_PATH, &tls_client_crt, &tls_client_crt_len);
        if (ret > 0)
        {
            ret = load_credential_from_fs(PRIVATE_KEY_PATH, &tls_client_key, &tls_client_key_len);
            if (ret > 0)
            {
                break;
            }
        }

        k_sleep(K_SECONDS(5));
    }

    if (tls_client_crt != NULL && tls_client_key != NULL)
    {
        struct golioth_client_config client_config = {
            .credentials =
                {
                    .auth_type = GOLIOTH_TLS_AUTH_TYPE_PKI,
                    .pki =
                        {
                            .ca_cert = tls_ca_crt,
                            .ca_cert_len = sizeof(tls_ca_crt),
                            .public_cert = tls_client_crt,
                            .public_cert_len = tls_client_crt_len,
                            .private_key = tls_client_key,
                            .private_key_len = tls_client_key_len,
                            .secondary_ca_cert = tls_secondary_ca_crt,
                            .secondary_ca_cert_len = sizeof(tls_secondary_ca_crt),
                        },
                },
        };

        client = golioth_client_create(&client_config);

        golioth_client_register_event_callback(client, on_client_event, NULL);
    }
    else
    {
        LOG_ERR("Error reading certificate credentials from filesystem");
    }

    k_sem_take(&connected, K_FOREVER);

    while (true)
    {
        LOG_INF("Sending hello! %d", counter);

        ++counter;
        k_sleep(K_SECONDS(5));
    }

    return 0;
}
