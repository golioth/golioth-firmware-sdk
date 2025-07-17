/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zephyr_coap_utils.h"

#include <zephyr/net/coap.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(golioth_coap_client);

#define COAP_BASIC_HEADER_SIZE 4

static inline uint8_t coap_data_get_token_len(uint8_t *data)
{
    return data[0] & 0x0f;
}

static inline uint8_t coap_data_get_type(uint8_t *data)
{
    return (data[0] & 0x30) >> 4;
}

static inline uint8_t coap_data_get_code(uint8_t *data)
{
    return data[1];
}

int coap_data_check_rx_packet_type(uint8_t *data, size_t len)
{
    uint8_t tkl;

    if (!data)
    {
        return -EINVAL;
    }

    if (len < COAP_BASIC_HEADER_SIZE)
    {
        return -EINVAL;
    }

    /* Token lengths 9-15 are reserved. */
    tkl = coap_data_get_token_len(data);
    if (tkl > 8)
    {
        GLTH_LOGD("Invalid RX");
        return -EINVAL;
    }

    if (tkl == 0 && len == COAP_BASIC_HEADER_SIZE && coap_data_get_type(data) == COAP_TYPE_CON
        && coap_data_get_code(data) == COAP_CODE_EMPTY)
    {
        /* Empty packet */
        GLTH_LOGD("RX Empty");
        return -ENOMSG;
    }

    GLTH_LOGD("RX Non-empty");

    return 0;
}

int coap_packet_append_uri_path_from_string_range(struct coap_packet *packet,
                                                  const char *begin,
                                                  const char *end)
{
    const char *p;
    int err;

    /* Trim preceding '/' */
    while (*begin == '/')
    {
        begin++;
    }

    p = begin;

    /* Split path into URI-Path options */
    while (p < end && *p != '\0')
    {
        if (*p == '/')
        {
            err = coap_packet_append_option(packet, COAP_OPTION_URI_PATH, begin, p - begin);
            if (err)
            {
                return err;
            }

            begin = p + 1;
        }

        p++;
    }

    /* Return early if the last segment is empty */
    if (begin == p)
    {
        return 0;
    }

    /* Append last segment */
    return coap_packet_append_option(packet, COAP_OPTION_URI_PATH, begin, p - begin);
}

int coap_packet_append_uri_path_from_pathv(struct coap_packet *packet, const uint8_t **pathv)
{
    int err;

    for (size_t i = 0; pathv[i]; i++)
    {
        err = coap_packet_append_uri_path_from_stringz(packet, pathv[i]);
        if (err)
        {
            return err;
        }
    }

    return 0;
}

size_t coap_pathv_estimate_alloc_len(const uint8_t **pathv)
{
    size_t total = 0;

    for (size_t i = 0; pathv[i]; i++)
    {
        size_t path_len = strlen(pathv[i]);

        /*
         * Exact delimiter length is dependent on path component length.
         *
         * The worst case is path components of 13 characters (see RFC 7252 "3.1 Option
         * Format"), as that results in 1 byte of delta and 1 byte of length for each
         * component, resulting in 13 + 1 + 1 = 15 bytes per component.
         *
         * Each '/' already counts as 1 byte calculated within strlen(). Such character can
         * be subtracted from above, giving 1 byte of overhead per 13 characters.
         */
        total += path_len + (path_len / 13) + 1;
    }

    return total;
}
