#include "libcoap_posix_timers.h"
#include <FreeRTOS.h>
#include <task.h>

static uint32_t millis(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

int clock_gettime(clockid_t clock_id, struct timespec* tp) {
    uint32_t now_ms = millis();
    tp->tv_sec = now_ms / 1000ul;
    tp->tv_nsec = (now_ms % 1000ul) * 1000000ul;
    return 0;
}
