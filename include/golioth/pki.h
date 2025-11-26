/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __cplusplus
extern "C"
{
#endif

#pragma once

#include <stdint.h>
#include <golioth/golioth_status.h>
#include <golioth/client.h>
#include <golioth/config.h>

/// @defgroup golioth_pki golioth_pki
/// Functions for interacting with Golioth Public Key Infrastructure services
///
/// @{

/// Request a new certificate from the configured PKI provider
///
/// Posts a Certificate Signing Request (CSR) to the PKI provider, which returns
/// a signed certificate for the device.
///
/// The CSR must be a DER encoded PKCS #10 object signed by the private key the
/// device uses for authenticating with Golioth. If successful, a DER encoded
/// X.509 certificate is returned.
///
/// All fields in the CSR subject are optional, but if present, the
/// Organization and Common Name fields must contain the device's project and
/// certificate ID, respectively. The configured PKI provider may have additional
/// restrictions on the CSR's subject, format or extensions.
///
/// @param client The client handle from @ref golioth_client_create
/// @param csr A DER formatted Certificate Signing Request.
/// @param csr_size The size of the CSR
/// @param callback Callback to call on response received.
/// @param callback_arg Callback argument, passed directly when callback invoked. Can be NULL.
enum golioth_status golioth_pki_issue_cert(struct golioth_client *client,
                                           const uint8_t *csr,
                                           size_t csr_size,
                                           golioth_post_cb_fn callback,
                                           void *callback_arg);

/// @}

#ifdef __cplusplus
}
#endif
