#pragma once

#include "golioth_client.h"
#include <stdbool.h>

/// @defgroup golioth_remote_shell golioth_remote_shell
/// Functions for interacting with Golioth Remote Shell.
///
/// The remote shell allows for remote I/O via a shell-like
/// interface. Output of the device that would normally be
/// transmitted over serial will also be sent to Golioth.
/// Additionally, input lines of text can be received from
/// Golioth via RPC and fed into the platform shell for execution.
///
/// @{

/// Callback for handling a line of input text from Golioth
///
/// It is intended that this callback feed the line to the shell thread
/// for execution.
///
/// @param line The line of text (is NULL-terminated)
/// @param line_len Length of line, not including NULL.
typedef void (*remote_shell_input_cb)(const char* line, size_t line_len);

/// Enable or disable the remote shell feature.
///
/// @param enable Enable shell feature (if true)
void golioth_remote_shell_set_enable(bool enable);

/// Set callback to call when a line of input text is received from Golioth
///
/// @param cb Line input handler
void golioth_remote_shell_set_line_input_handler(remote_shell_input_cb cb);

/// Internal function to set the Golioth client.
///
/// Users should not call this directly, as it is called implicitly
/// creation of the Golioth client.
///
/// This client will be used to publish log data to LightDB stream.
///
/// @param client Client handle
void golioth_remote_shell_set_client(golioth_client_t client);

/// Internal function to push bytes to the shell output ringbuffer.
///
/// Users should not call this directly. It is meant to be called
/// by the port layer, when bytes are sent to stdout/serial.
///
/// If the ringbuffer is full, bytes may be dropped.
///
/// @param data Bytes to push to ringbuffer
/// @param nbytes Number of bytes to push
void golioth_remote_shell_push(const uint8_t* data, size_t nbytes);

/// @}
