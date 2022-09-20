#pragma once

#include "golioth_time.h"

#define ESP_LOGV(TAG, ...)
#if 0
#define ESP_LOGV(TAG, ...) \
    do { \
        printf("%s (%lu) %s: ", "V", (uint32_t)golioth_time_millis(), TAG); \
        printf(__VA_ARGS__); \
        puts(""); \
    } while (0)
#endif

#if 1
#define ESP_LOGD(TAG, ...) \
    do { \
        printf("%s (%lu) %s: ", "D", (uint32_t)golioth_time_millis(), TAG); \
        printf(__VA_ARGS__); \
        puts(""); \
    } while (0)

#define ESP_LOGI(TAG, ...) \
    do { \
        printf("%s (%lu) %s: ", "I", (uint32_t)golioth_time_millis(), TAG); \
        printf(__VA_ARGS__); \
        puts(""); \
    } while (0)

#define ESP_LOGW(TAG, ...) \
    do { \
        printf("%s (%lu) %s: ", "W", (uint32_t)golioth_time_millis(), TAG); \
        printf(__VA_ARGS__); \
        puts(""); \
    } while (0)

#define ESP_LOGE(TAG, ...) \
    do { \
        printf("%s (%lu) %s: ", "E", (uint32_t)golioth_time_millis(), TAG); \
        printf(__VA_ARGS__); \
        puts(""); \
    } while (0)

#define ESP_LOG_BUFFER_HEXDUMP(TAG, ...) /* TODO */
#else
#define ESP_LOGD(TAG, ...)
#define ESP_LOGI(TAG, ...)
#define ESP_LOGW(TAG, ...)
#define ESP_LOGE(TAG, ...)
#define ESP_LOG_BUFFER_HEXDUMP(TAG, ...)
#endif
