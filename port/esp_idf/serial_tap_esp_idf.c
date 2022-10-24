#include "hal/uart_hal.h"
#include "golioth_remote_shell.h"
#include <stdint.h>

extern void __real_uart_hal_write_txfifo(
        uart_hal_context_t* hal,
        const uint8_t* buf,
        uint32_t data_size,
        uint32_t* write_size);

void __wrap_uart_hal_write_txfifo(
        uart_hal_context_t* hal,
        const uint8_t* buf,
        uint32_t data_size,
        uint32_t* write_size) {
    golioth_remote_shell_push(buf, data_size);
    __real_uart_hal_write_txfifo(hal, buf, data_size, write_size);
}
