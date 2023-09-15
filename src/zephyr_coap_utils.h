/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <zephyr/net/coap.h>

/**
 * Check CoAP packet type based on raw data received.
 *
 * @retval -EINVAL invalid message
 * @retval -ENOMSG empty CoAP message (ping)
 * @retval 0 valid CoAP packet (to be parsed with)
 */
int coap_data_check_rx_packet_type(uint8_t *data, size_t len);

static inline void coap_packet_set_id(struct coap_packet *packet, uint16_t id)
{
    packet->data[2] = id >> 8;
    packet->data[3] = id & 0xff;
}

int coap_packet_append_uri_path_from_string_range(struct coap_packet *packet,
                                                  const char *begin,
                                                  const char *end);

static inline int coap_packet_append_uri_path_from_string(struct coap_packet *packet,
                                                          const char *path,
                                                          size_t path_len)
{
    return coap_packet_append_uri_path_from_string_range(packet, path, path + path_len);
}

static inline int coap_packet_append_uri_path_from_stringz(struct coap_packet *packet,
                                                           const char *path)
{
    return coap_packet_append_uri_path_from_string_range(packet, path, (void *) UINTPTR_MAX);
}

int coap_packet_append_uri_path_from_pathv(struct coap_packet *packet, const uint8_t **pathv);

size_t coap_pathv_estimate_alloc_len(const uint8_t **pathv);
