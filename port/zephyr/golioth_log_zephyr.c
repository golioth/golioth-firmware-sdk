/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <golioth/log.h>
#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/logging/log_output.h>
#include <zephyr/sys/cbprintf.h>

/* Set this to 1 if you want to see what is being sent to server */
#define DEBUG_PRINTING 0

#if DEBUG_PRINTING
#define DBG(fmt, ...) printk("%s: " fmt, __func__, ##__VA_ARGS__)
#else
#define DBG(fmt, ...)
#endif

static const struct log_backend log_backend_golioth;

struct cbpprintf_ctx {
    size_t ctr;
    char msg[CONFIG_LOG_BACKEND_GOLIOTH_MAX_LOG_STRING_SIZE];
};

struct golioth_log_ctx {
    struct golioth_client* client;
    bool panic_mode;
    struct cbpprintf_ctx print_ctx;
};

static struct golioth_log_ctx log_ctx;

typedef golioth_status_t (*glth_log_fn)(struct golioth_client*, const char*, const char*, int32_t);

static golioth_status_t golioth_log_drop(
        struct golioth_client* client,
        const char* tag,
        const char* log_message,
        int32_t timeout_s) {
    return GOLIOTH_OK;
}

static const glth_log_fn log_to_log_func(struct log_msg* log) {
    int level = log_msg_get_level(log);

    switch (level) {
    case LOG_LEVEL_ERR:
        return golioth_log_error_sync;
    case LOG_LEVEL_WRN:
        return golioth_log_warn_sync;
    case LOG_LEVEL_INF:
        return golioth_log_info_sync;
    case LOG_LEVEL_DBG:
        return golioth_log_debug_sync;
    default:
        return golioth_log_drop;
    }
}

static int cbpprintf_out_func(int c, void* out_ctx) {
    struct cbpprintf_ctx* ctx = out_ctx;
    if (ctx->ctr >= CONFIG_LOG_BACKEND_GOLIOTH_MAX_LOG_STRING_SIZE) {
        return -ENOMEM;
    }
    ctx->msg[ctx->ctr++] = c;

    return 0;
}

static void process(const struct log_backend* const backend, union log_msg_generic* msg) {
    const char* module = NULL;
    struct log_msg* log = &msg->log;
    struct golioth_log_ctx* ctx = backend->cb->ctx;

    if (ctx->panic_mode) {
        return;
    }

    const void* source = log_msg_get_source(log);
    if (source) {
        uint8_t domain_id = log_msg_get_domain(log);
        int16_t source_id =
            (IS_ENABLED(CONFIG_LOG_RUNTIME_FILTERING) ? log_dynamic_source_id((void*)source)
                                                      : log_const_source_id(source));
        module = log_source_name_get(domain_id, source_id);
    }

    size_t len;
    void* data = log_msg_get_package(log, &len);
    ctx->print_ctx.ctr = 0;
    cbpprintf(cbpprintf_out_func, &ctx->print_ctx, data);
    ctx->print_ctx.msg[ctx->print_ctx.ctr] = '\0';
    log_to_log_func(log)(
        ctx->client, module, ctx->print_ctx.msg, GOLIOTH_SYS_WAIT_FOREVER);
}

static void init(const struct log_backend* const backend) {
    log_backend_deactivate(backend);
}

static void panic(struct log_backend const* const backend) {
    struct golioth_log_ctx *ctx = backend->cb->ctx;
    ctx->panic_mode = true;
}

static void dropped(const struct log_backend* const backend, uint32_t cnt) {}

static const struct log_backend_api log_backend_golioth_api = {
    .panic = panic,
    .init = init,
    .process = process,
    .dropped = dropped,
};

/* Note that the backend can be activated only after we have networking
 * subsystem ready so we must not start it immediately.
 */
LOG_BACKEND_DEFINE(log_backend_golioth, log_backend_golioth_api, false);

int log_backend_golioth_disable(void* client) {
    log_backend_disable(&log_backend_golioth);

    return 0;
}

int log_backend_golioth_enable(void* client) {
    if (log_ctx.client == NULL) {
        log_ctx.client = client;
    }
    log_backend_enable(&log_backend_golioth, &log_ctx, CONFIG_LOG_MAX_LEVEL);

    return 0;
}
