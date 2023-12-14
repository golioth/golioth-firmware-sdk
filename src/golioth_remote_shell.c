#include "golioth_remote_shell.h"

#if (CONFIG_GOLIOTH_REMOTE_SHELL_ENABLE == 1)

#include "golioth_ringbuf.h"
#include "golioth_util.h"
#include "golioth_debug.h"
#include "golioth_lightdb.h"
#include "golioth_rpc.h"
#include "golioth_config.h"
#include "golioth_sys.h"
#include <assert.h>
#include <string.h>
#include <zcbor_encode.h>

LOG_TAG_DEFINE(golioth_remote_shell);

static bool _initialized;
static bool _enable = true;
static golioth_client_t _client;
static remote_shell_input_cb _line_input_cb;
size_t _bytes_written = 0;
size_t _bytes_read = 0;
size_t _bytes_dropped = 0;
RINGBUF_DEFINE(_log_ringbuf, 1, CONFIG_GOLIOTH_REMOTE_SHELL_BUF_SIZE);

static void remote_shell_thread(void* arg) {
    const uint16_t max_bytes_to_read = 512;
    uint8_t encode_buf[max_bytes_to_read + 8];  // encoded, this is what is sent to Golioth
    char logstr[max_bytes_to_read];             // raw bytes, read from _log_ringbuf, plus NULL
    bool ok;

    while (1) {
        if (_enable && _client) {
            int32_t nbytes = ringbuf_size(&_log_ringbuf);

            // Pull bytes out of log queue, chunks at a time
            while (nbytes > 0) {
                ZCBOR_STATE_E(zse, 1, encode_buf, sizeof(encode_buf), 1);
                uint16_t bytes_to_read = min(nbytes, max_bytes_to_read);

                // Copy bytes into buffer.
                for (uint16_t i = 0; i < bytes_to_read; i++) {
                    ringbuf_get(&_log_ringbuf, &logstr[i]);
                    _bytes_read++;
                }

                ok = zcbor_map_start_encode(zse, 1);
                if (!ok) {
                    GLTH_LOGE(TAG, "Failed to encode map");
                    return;
                }

                ok = zcbor_tstr_put_lit(zse, "s")
                        && zcbor_tstr_encode_ptr(zse, logstr, bytes_to_read);
                if (!ok) {
                    GLTH_LOGE(TAG, "Failed to encode log data");
                    return;
                }

                ok = zcbor_map_end_encode(zse, 1);
                if (!ok) {
                    GLTH_LOGE(TAG, "Failed to encode end of map");
                    return;
                }

                // post to LightDB stream, path .s/shell
                GLTH_LOGD(
                        TAG,
                        "Sending %zu bytes to LightDB stream",
                        (size_t)(zse->payload - encode_buf));
                golioth_status_t status = golioth_lightdb_stream_set_cbor_sync(
                        _client,
                        "shell",
                        encode_buf,
                        zse->payload - encode_buf,
                        GOLIOTH_SYS_WAIT_FOREVER);
                if (status != GOLIOTH_OK) {
                    GLTH_LOGE(TAG, "Failed to post log data");
                }

                nbytes -= bytes_to_read;
            }
        }
        GLTH_LOGD(TAG, "wr = %u, rd = %u, drop = %u", _bytes_written, _bytes_read, _bytes_dropped);
        golioth_sys_msleep(CONFIG_GOLIOTH_REMOTE_SHELL_THREAD_DELAY_MS);
    };
}

static golioth_rpc_status_t on_line_input(
        zcbor_state_t* request_params_array,
        zcbor_state_t* response_detail_map,
        void* callback_arg) {
    if (_line_input_cb) {
        struct zcbor_string tstr;
        bool ok = zcbor_tstr_decode(request_params_array, &tstr);
        if (!ok) {
            return GOLIOTH_RPC_INVALID_ARGUMENT;
        }
        _line_input_cb((char*)tstr.value, tstr.len);
    }
    return GOLIOTH_RPC_OK;
}

static void init(void) {
    // Create thread that will pull items out of _log_ringbuf
    golioth_sys_thread_t thread = golioth_sys_thread_create((golioth_sys_thread_config_t){
            .name = "remote_shell",
            .fn = remote_shell_thread,
            .user_arg = NULL,
            .stack_size = CONFIG_GOLIOTH_REMOTE_SHELL_THREAD_STACK_SIZE,
            .prio = CONFIG_GOLIOTH_COAP_THREAD_PRIORITY});
    if (!thread) {
        GLTH_LOGE(TAG, "Failed to create remote shell thread");
        return;
    }
}

void golioth_remote_shell_push(const uint8_t* data, size_t nbytes) {
    if (!_enable) {
        return;
    }

    assert(nbytes <= CONFIG_GOLIOTH_REMOTE_SHELL_BUF_SIZE);

    // Push bytes to queue
    for (size_t i = 0; i < nbytes; i++) {
        bool success = ringbuf_put(&_log_ringbuf, &data[i]);
        if (success) {
            _bytes_written++;
        } else {
            _bytes_dropped++;
        }
    }
}

void golioth_remote_shell_set_enable(bool enable) {
    if (_enable & !enable) {
        // Disabling (was enabled, now it's not)
        //
        // In this case, reset the ringbuf.
        // If it's ever re-enabled in the future, this will prevent
        // old logs from being in the ringbuf.
        ringbuf_reset(&_log_ringbuf);
    }
    _enable = enable;
}

void golioth_remote_shell_set_client(golioth_client_t client) {
    if (!_initialized) {
        init();
        _initialized = true;
    }

    _client = client;
    golioth_rpc_register(client, "line_input", on_line_input, NULL);
}

void golioth_remote_shell_set_line_input_handler(remote_shell_input_cb cb) {
    _line_input_cb = cb;
}

#else  // !CONFIG_GOLIOTH_REMOTE_SHELL_ENABLE

// Stubs if shell not enabled
void golioth_remote_shell_set_client(golioth_client_t client) {}
void golioth_remote_shell_set_enable(bool enable) {}
void golioth_remote_shell_push(const uint8_t* data, size_t nbytes) {}
void golioth_remote_shell_set_line_input_handler(remote_shell_input_cb cb) {}

#endif
