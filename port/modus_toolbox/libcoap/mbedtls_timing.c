#include "mbedtls/timing.h"
#include <FreeRTOS.h>
#include <task.h>

struct _hr_time {
    uint32_t start_ms;
};

static uint32_t millis(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

unsigned long mbedtls_timing_get_timer(struct mbedtls_timing_hr_time* val, int reset) {
    struct _hr_time* t = (struct _hr_time*)val;

    if (reset) {
        t->start_ms = millis();
        return 0;
    } else {
        return (millis() - t->start_ms);
    }
}

void mbedtls_timing_set_delay(void* data, uint32_t int_ms, uint32_t fin_ms) {
    mbedtls_timing_delay_context* ctx = (mbedtls_timing_delay_context*)data;

    ctx->int_ms = int_ms;
    ctx->fin_ms = fin_ms;

    if (fin_ms != 0) {
        (void)mbedtls_timing_get_timer(&ctx->timer, 1);
    }
}

int mbedtls_timing_get_delay(void* data) {
    mbedtls_timing_delay_context* ctx = (mbedtls_timing_delay_context*)data;
    unsigned long elapsed_ms;

    if (ctx->fin_ms == 0) {
        return -1;
    }

    elapsed_ms = mbedtls_timing_get_timer(&ctx->timer, 0);

    if (elapsed_ms >= ctx->fin_ms) {
        return 2;
    }

    if (elapsed_ms >= ctx->int_ms) {
        return 1;
    }

    return 0;
}
