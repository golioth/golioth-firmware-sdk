/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <golioth/pki.h>
#include "coap_client.h"
#include "golioth/client.h"

#if defined(CONFIG_GOLIOTH_PKI)

#define GOLIOTH_PKI_CSR_PATH "v1/csr"

enum golioth_status golioth_pki_issue_cert(struct golioth_client *client,
                                           const uint8_t *csr,
                                           size_t csr_size,
                                           golioth_post_cb_fn callback,
                                           void *callback_arg)
{
    if (csr == NULL || callback == NULL)
    {
        return GOLIOTH_ERR_NULL;
    }

    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    return golioth_coap_client_post(client,
                                    token,
                                    "",  // no prefix
                                    GOLIOTH_PKI_CSR_PATH,
                                    GOLIOTH_CONTENT_TYPE_OCTET_STREAM,
                                    csr,
                                    csr_size,
                                    callback,
                                    callback_arg,
                                    false,
                                    GOLIOTH_SYS_WAIT_FOREVER);
}


#endif  // CONFIG_GOLIOTH_PKI
