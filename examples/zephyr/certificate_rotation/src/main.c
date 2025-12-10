/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cert_rotation, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/pki.h>
#include <samples/common/net_connect.h>
#include <zephyr/kernel.h>
#include <zephyr/net/coap.h>
#include <zephyr/device.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/sys/reboot.h>
#include <mbedtls/x509_csr.h>
#include <mbedtls/psa_util.h>

#define CRT_PATH "/lfs1/credentials/crt.der"
#define KEY_PATH "/lfs1/credentials/key.der"

static const uint8_t tls_ca_crt[] = {
#include "golioth-systemclient-ca_crt.inc"
};

static const uint8_t tls_secondary_ca_crt[] = {
#if CONFIG_GOLIOTH_SECONDARY_CA_CRT
#include "golioth-systemclient-secondary_ca_crt.inc"
#endif
};

#define CSR_BUF_SIZE 300

struct golioth_client *client;
static K_SEM_DEFINE(connected, 0, 1);

static struct
{
    struct k_sem valid;
    uint8_t *buf;
    size_t len;
} new_cert;

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
     * allocated for the lifetime of the session.
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

static void recv_cert(struct golioth_client *client,
                      enum golioth_status status,
                      const struct golioth_coap_rsp_code *coap_rsp_code,
                      const char *path,
                      const uint8_t *payload,
                      size_t payload_size,
                      void *arg)
{
    if (status != GOLIOTH_OK)
    {
        LOG_ERR("Request failed: %d (CoAP status %d.%02d)",
                status,
                coap_rsp_code->code_class,
                coap_rsp_code->code_detail);
        return;
    }

    // Got a signed certificate based on our Certificate Signing Request:
    new_cert.buf = malloc(payload_size);
    if (new_cert.buf == NULL)
    {
        LOG_ERR("Failed to allocate memory for new certificate");
        return;
    }

    memcpy(new_cert.buf, payload, payload_size);
    new_cert.len = payload_size;

    k_sem_give(&new_cert.valid);
}

static int start_certificate_rotation(const uint8_t *csr, size_t size)
{
    k_sem_init(&new_cert.valid, 0, 1);

    return golioth_pki_issue_cert(client, csr, size, recv_cert, CRT_PATH);
}

static uint8_t *create_csr(const uint8_t *pkey, size_t pkey_len, size_t *len)
{
    int err;
    /* Import the key pair into an mbedtls pk context. This API also supports pulling
     * the key from the PSA storage.
     */
    struct mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    err = mbedtls_pk_parse_key(&pk,
                               pkey,
                               pkey_len,
                               NULL,
                               0,
                               mbedtls_psa_get_random,
                               MBEDTLS_PSA_RANDOM_STATE);
    if (err)
    {
        LOG_ERR("Unable to parse key: %d", err);
        return NULL;
    }

    // Create a Certificate Signing Request:
    struct mbedtls_x509write_csr csr;
    mbedtls_x509write_csr_init(&csr);

    mbedtls_x509write_csr_set_key(&csr, &pk);
    mbedtls_x509write_csr_set_md_alg(&csr, MBEDTLS_MD_SHA256);

    uint8_t *der = malloc(CSR_BUF_SIZE);
    if (der == NULL)
    {
        LOG_ERR("Failed to allocate memory for csr");
        return NULL;
    }

    // The API requires a DER formatted version of the CSR:
    int res = mbedtls_x509write_csr_der(&csr,
                                        der,
                                        CSR_BUF_SIZE,
                                        mbedtls_psa_get_random,
                                        MBEDTLS_PSA_RANDOM_STATE);
    if (res < 0)
    {
        LOG_ERR("Failed to write CSR to DER: %#x", -res);
        mbedtls_x509write_csr_free(&csr);
        mbedtls_pk_free(&pk);
        free(der);
        return NULL;
    }

    *len = res;
    mbedtls_x509write_csr_free(&csr);
    mbedtls_pk_free(&pk);

    // The csr is written at the end of the available buffer, so we need to return with an offset:
    return &der[CSR_BUF_SIZE - res];
}

int main(void)
{
    LOG_DBG("Start certificate rotation sample");

    net_connect();

    uint8_t *tls_client_crt = NULL;
    uint8_t *tls_client_key = NULL;
    size_t tls_client_crt_len, tls_client_key_len;

    while (1)
    {
        int ret = load_credential_from_fs(CRT_PATH, &tls_client_crt, &tls_client_crt_len);
        if (ret > 0)
        {
            ret = load_credential_from_fs(KEY_PATH, &tls_client_key, &tls_client_key_len);
            if (ret > 0)
            {
                break;
            }
        }

        k_sleep(K_SECONDS(5));
    }

    if (tls_client_crt == NULL || tls_client_key == NULL)
    {
        LOG_ERR("Error reading certificate credentials from filesystem");
        return 0;
    }

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

    k_sem_take(&connected, K_FOREVER);

    /**
     * Rotate the certificate by submitting a Certificate Signing Request to Golioth.
     * The CSR does not have to contain the project or device ID, as this will be
     * inferred from the current credentials in the Golioth backend.
     */
    size_t csr_size = 0;
    uint8_t *csr = create_csr(tls_client_key, tls_client_key_len, &csr_size);
    if (csr == NULL)
    {
        return 0;
    }

    int err = start_certificate_rotation(csr, csr_size);
    if (err)
    {
        LOG_ERR("Certificate rotation failed: %d", err);
        free(csr);
        return 0;
    }

    k_sem_take(&new_cert.valid, K_FOREVER);
    free(csr);

    LOG_INF("Received new certificate. Reconnecting...");

    golioth_client_destroy(client);

    // We no longer need the old certificate:
    free(tls_client_crt);
    tls_client_crt = NULL;

    // Use the new certificate:
    client_config.credentials.pki.public_cert = new_cert.buf;
    client_config.credentials.pki.public_cert_len = new_cert.len;

    // Reconnect:
    k_sem_init(&connected, 0, 1);
    client = golioth_client_create(&client_config);
    if (client == NULL)
    {
        LOG_ERR("Failed to create new golioth client");
        return 0;
    }

    golioth_client_register_event_callback(client, on_client_event, NULL);

    k_sem_take(&connected, K_FOREVER);

    LOG_INF("Reconnected.");

    while (true)
    {
        LOG_INF("Sending with new certificate");
        k_sleep(K_SECONDS(5));
    }

    return 0;
}
