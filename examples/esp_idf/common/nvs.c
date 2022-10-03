/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "nvs.h"

#define TAG "nvs"

static const char* read_nvs_key_or_default(
        const char* key,
        char* out,
        size_t outsize,
        const char* defaultstr) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed");
        return defaultstr;
    }
    size_t bytes_read = outsize;
    err = nvs_get_str(handle, key, out, &bytes_read);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "nvs_get_str key %s failed, err = %d", key, err);
    }
    nvs_close(handle);
    return (err == ESP_OK ? out : defaultstr);
}

void nvs_init(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
}

const char* nvs_read_wifi_ssid(void) {
    static char buf[32];
    return read_nvs_key_or_default(NVS_WIFI_SSID_KEY, buf, sizeof(buf), NVS_DEFAULT_STR);
}

const char* nvs_read_wifi_password(void) {
    static char buf[32];
    return read_nvs_key_or_default(NVS_WIFI_PASS_KEY, buf, sizeof(buf), NVS_DEFAULT_STR);
}

const char* nvs_read_golioth_psk_id(void) {
    static char buf[128];
    return read_nvs_key_or_default(NVS_GOLIOTH_PSK_ID_KEY, buf, sizeof(buf), NVS_DEFAULT_STR);
}

const char* nvs_read_golioth_psk(void) {
    static char buf[128];
    return read_nvs_key_or_default(NVS_GOLIOTH_PSK_KEY, buf, sizeof(buf), NVS_DEFAULT_STR);
}

const char* nvs_read_str(const char* key, char* buf, size_t bufsize) {
    return read_nvs_key_or_default(key, buf, bufsize, NVS_DEFAULT_STR);
}

bool nvs_write_str(const char* key, const char* str) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed");
        return false;
    }
    err = nvs_set_str(handle, key, str);
    nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set %s to %s, err = %d", key, str, err);
        return false;
    }
    return true;
}

bool nvs_erase_str(const char* key) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed");
        return false;
    }
    err = nvs_erase_key(handle, key);
    nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase key %s, err = %d", key, err);
        return false;
    }
    return true;
}

bool nvs_credentials_are_set(void) {
    if (0 == strcmp(nvs_read_wifi_ssid(), NVS_DEFAULT_STR)) {
        return false;
    }
    if (0 == strcmp(nvs_read_wifi_password(), NVS_DEFAULT_STR)) {
        return false;
    }
    if (0 == strcmp(nvs_read_golioth_psk_id(), NVS_DEFAULT_STR)) {
        return false;
    }
    if (0 == strcmp(nvs_read_golioth_psk(), NVS_DEFAULT_STR)) {
        return false;
    }
    return true;
}
