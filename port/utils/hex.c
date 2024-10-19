/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * These utilities already included in Zephyr tree but needed for other platforms for OTA service.
 */

#include <errno.h>
#include "hex.h"

static int char2hex(char c, uint8_t *x)
{
    if ((c >= '0') && (c <= '9'))
    {
        *x = c - '0';
    }
    else if ((c >= 'a') && (c <= 'f'))
    {
        *x = c - 'a' + 10;
    }
    else if ((c >= 'A') && (c <= 'F'))
    {
        *x = c - 'A' + 10;
    }
    else
    {
        return -EINVAL;
    }

    return 0;
}

size_t hex2bin(const char *hex, size_t hexlen, uint8_t *buf, size_t buflen)
{
    uint8_t dec;

    if (buflen < (hexlen / 2U + hexlen % 2U))
    {
        return 0;
    }

    /* if hexlen is uneven, insert leading zero nibble */
    if ((hexlen % 2U) != 0)
    {
        if (char2hex(hex[0], &dec) < 0)
        {
            return 0;
        }
        buf[0] = dec;
        hex++;
        buf++;
    }

    /* regular hex conversion */
    for (size_t i = 0; i < (hexlen / 2U); i++)
    {
        if (char2hex(hex[2U * i], &dec) < 0)
        {
            return 0;
        }
        buf[i] = dec << 4;

        if (char2hex(hex[2U * i + 1U], &dec) < 0)
        {
            return 0;
        }
        buf[i] += dec;
    }

    return hexlen / 2U + hexlen % 2U;
}
