#pragma once

#include <stdint.h>
#include <stddef.h>
#include "golioth_client.h"

/// Create queue for packet data.
/// Create task for pull data out of queue.
void golioth_packet_capture_init(void);

/// Push data to the packet queue
void golioth_packet_capture_push(const uint8_t* data, size_t data_len);

void golioth_packet_capture_set_client(golioth_client_t client);

void golioth_packet_capture_set_enable(bool enable);
