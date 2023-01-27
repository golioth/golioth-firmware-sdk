/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef COAP_ZEPHYR_H_
#define COAP_ZEPHYR_H_

/**
 * Connect to DTLS server (including handshake).
 *
 * @param session CoAP session.
 *
 * @retval 1 if successful
 * @retval 0 if failed
 */
int coap_dtls_zephyr_connect(coap_session_t* session);

#endif /* COAP_ZEPHYR_H_ */
