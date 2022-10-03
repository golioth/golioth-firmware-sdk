/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdlib.h>
#include <stdbool.h>

// Namespace to use for NVS keys. Can be anything.
#define NVS_NAMESPACE "example"

// NVS keys for wifi credentials
#define NVS_WIFI_SSID_KEY "w_ssid"
#define NVS_WIFI_PASS_KEY "w_pass"
#define NVS_GOLIOTH_PSK_ID_KEY "g_psk_id"
#define NVS_GOLIOTH_PSK_KEY "g_psk"

// Default string value to return in nvs_read_* functions if key not found in NVS
#define NVS_DEFAULT_STR "unknown"

void nvs_init(void);

const char* nvs_read_wifi_ssid(void);
const char* nvs_read_wifi_password(void);
const char* nvs_read_golioth_psk_id(void);
const char* nvs_read_golioth_psk(void);

const char* nvs_read_str(const char* key, char* buf, size_t bufsize);
bool nvs_write_str(const char* key, const char* str);
bool nvs_erase_str(const char* key);

bool nvs_credentials_are_set(void);
