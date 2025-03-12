/*
 * Copyright (c) 2021-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_wifi, LOG_LEVEL_INF);

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/pm/device.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>

#define WIFI_MANAGER_MGMT_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT)

#define NET_STATE_WIFI_CONNECTED BIT(0)
#define NET_STATE_WIFI_CONNECTING BIT(1)
#define NET_STATE_IPV4_READY BIT(2)

enum wifi_state
{
    WIFI_STATE_IDLE,
    WIFI_STATE_WAITING_FOR_LOWER_UP,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_DISCONNECTING,
    WIFI_STATE_READY,
    WIFI_STATE_LAST = WIFI_STATE_READY,
    WIFI_STATE_INVALID,
    WIFI_STATE_WAIT,
};

enum wifi_event
{
    WIFI_EVENT_START,
    WIFI_EVENT_LOWER_UP,
    WIFI_EVENT_CONNECTED,
    WIFI_EVENT_DISCONNECTED,
    WIFI_EVENT_IP_ADD,
    WIFI_EVENT_IP_DEL,
};

struct wifi_manager_config
{
    k_thread_stack_t *workq_stack;
    size_t workq_stack_size;
    struct k_work_queue_config workq_config;
};

enum wifi_manager_flags
{
    WIFI_FLAG_ENABLE,
    WIFI_FLAG_RECONNECT,
};

struct wifi_manager_data
{
    struct net_if *iface;

    struct k_work_q workq;
    enum wifi_state state;

    atomic_t net_state;
    struct k_work net_mgmt_event_work;
    int net_state_old;

    atomic_t flags;

    enum wifi_event event;
    struct k_work event_work;

    struct net_mgmt_event_callback ipv4_mgmt_cb;
    struct net_mgmt_event_callback wifi_mgmt_cb;
};

static const char *wifi_state_str(enum wifi_state state)
{
    switch (state)
    {
        case WIFI_STATE_IDLE:
            return "IDLE";
        case WIFI_STATE_WAITING_FOR_LOWER_UP:
            return "WAIT_FOR_LOWER_UP";
        case WIFI_STATE_CONNECTING:
            return "CONNECTING";
        case WIFI_STATE_DISCONNECTING:
            return "DISCONNECTING";
        case WIFI_STATE_READY:
            return "READY";
        case WIFI_STATE_INVALID:
            return "INVALID";
        case WIFI_STATE_WAIT:
        default:
            return "WAIT";
    }

    return "";
}

static const char *wifi_event_str(enum wifi_event event)
{
    switch (event)
    {
        case WIFI_EVENT_START:
            return "START";
        case WIFI_EVENT_LOWER_UP:
            return "LOWER_UP";
        case WIFI_EVENT_CONNECTED:
            return "CONNECTED";
        case WIFI_EVENT_DISCONNECTED:
            return "DISCONNECTED";
        case WIFI_EVENT_IP_ADD:
            return "IP_ADD";
        case WIFI_EVENT_IP_DEL:
            return "IP_DEL";
    }

    return "";
}

#define MAP_OFFSET 1

#define IDL (WIFI_STATE_IDLE + MAP_OFFSET)
#define LUP (WIFI_STATE_WAITING_FOR_LOWER_UP + MAP_OFFSET)
#define CON (WIFI_STATE_CONNECTING + MAP_OFFSET)
#define DIS (WIFI_STATE_DISCONNECTING + MAP_OFFSET)
#define RDY (WIFI_STATE_READY + MAP_OFFSET)
#define INV (WIFI_STATE_INVALID + MAP_OFFSET)
#define WAIT(ms) (WIFI_STATE_WAIT + MAP_OFFSET + (ms))

static const int wifi_state_change_map[][WIFI_STATE_LAST + 1] = {
    /* IDL, LUP,       CON,         DIS,        RDY */
    [WIFI_EVENT_START] = {LUP, WAIT(100), INV, INV, INV},
    [WIFI_EVENT_LOWER_UP] = {INV, CON, INV, INV, INV},
    [WIFI_EVENT_CONNECTED] = {INV, INV, 0, WAIT(3000), 0},
    [WIFI_EVENT_DISCONNECTED] = {INV, INV, WAIT(1000), IDL, CON},
    [WIFI_EVENT_IP_ADD] = {INV, INV, RDY, RDY, 0},
    [WIFI_EVENT_IP_DEL] = {INV, INV, 0, 0, CON},
};

#if defined(CONFIG_GOLIOTH_SAMPLE_WIFI_SETTINGS)

static uint8_t wifi_ssid[WIFI_SSID_MAX_LEN];
static size_t wifi_ssid_len;
static uint8_t wifi_psk[WIFI_PSK_MAX_LEN];
static size_t wifi_psk_len;

static int wifi_settings_get(const char *name, char *dst, int val_len_max)
{
    uint8_t *val;
    size_t val_len;

    if (!strcmp(name, "ssid"))
    {
        val = wifi_ssid;
        val_len = wifi_ssid_len;
    }
    else if (!strcmp(name, "psk"))
    {
        val = wifi_psk;
        val_len = wifi_psk_len;
    }
    else
    {
        LOG_WRN("Unsupported key '%s'", name);
        return -ENOENT;
    }

    if (val_len > val_len_max)
    {
        LOG_ERR("Not enough space (%zu %d)", val_len, val_len_max);
        return -ENOMEM;
    }

    memcpy(dst, val, val_len);

    return val_len;
}

static int wifi_settings_set(const char *name,
                             size_t len_rd,
                             settings_read_cb read_cb,
                             void *cb_arg)
{
    uint8_t *buffer;
    size_t buffer_len;
    size_t *ret_len;
    ssize_t ret;

    if (!strcmp(name, "ssid"))
    {
        buffer = wifi_ssid;
        buffer_len = sizeof(wifi_ssid);
        ret_len = &wifi_ssid_len;
    }
    else if (!strcmp(name, "psk"))
    {
        buffer = wifi_psk;
        buffer_len = sizeof(wifi_psk);
        ret_len = &wifi_psk_len;
    }
    else
    {
        LOG_WRN("Unsupported key '%s'", name);
        return -ENOENT;
    }

    ret = read_cb(cb_arg, buffer, buffer_len);
    if (ret < 0)
    {
        LOG_ERR("Failed to read value: %d", (int) ret);
        return ret;
    }

    *ret_len = ret;

    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(wifi,
                               "wifi",
                               IS_ENABLED(CONFIG_SETTINGS_RUNTIME) ? wifi_settings_get : NULL,
                               wifi_settings_set,
                               NULL,
                               NULL);

#else /* defined(CONFIG_GOLIOTH_SAMPLE_WIFI_SETTINGS) */

static uint8_t wifi_ssid[] = CONFIG_GOLIOTH_SAMPLE_WIFI_SSID;
static size_t wifi_ssid_len = sizeof(CONFIG_GOLIOTH_SAMPLE_WIFI_SSID) - 1;
static uint8_t wifi_psk[] = CONFIG_GOLIOTH_SAMPLE_WIFI_PSK;
static size_t wifi_psk_len = sizeof(CONFIG_GOLIOTH_SAMPLE_WIFI_PSK) - 1;

#endif /* defined(CONFIG_GOLIOTH_SAMPLE_WIFI_SETTINGS) */

static void wifi_event_notify(struct wifi_manager_data *wifi_mgmt, enum wifi_event event)
{
    wifi_mgmt->event = event;
    k_work_submit_to_queue(&wifi_mgmt->workq, &wifi_mgmt->event_work);
}

/**
 * @brief Update CONNECTING flag from wifi_mgmt workqueue
 *
 * @note This function needs to be used only from within workqueue, as it
 * accesses both atomic 'net_state' and unprotected 'net_state_old'.
 * 'net_state_old' is altered to make clear that CONNECTING state is up-to-date
 * with workqueue processing logic.
 */
static void wifi_mgmt_connecting_update(struct wifi_manager_data *wifi_mgmt, bool connecting)
{
    if (connecting)
    {
        atomic_or(&wifi_mgmt->net_state, NET_STATE_WIFI_CONNECTING);
        wifi_mgmt->net_state_old |= NET_STATE_WIFI_CONNECTING;
    }
    else
    {
        atomic_and(&wifi_mgmt->net_state, ~NET_STATE_WIFI_CONNECTING);
        wifi_mgmt->net_state_old &= ~NET_STATE_WIFI_CONNECTING;
    }
}

static void wifi_connect(struct wifi_manager_data *wifi_mgmt)
{
    struct wifi_connect_req_params params = {
        .ssid = wifi_ssid,
        .ssid_length = wifi_ssid_len,
        .psk = wifi_psk,
        .psk_length = wifi_psk_len,
        .channel = WIFI_CHANNEL_ANY,
        .security = wifi_psk_len > 0 ? WIFI_SECURITY_TYPE_PSK : WIFI_SECURITY_TYPE_NONE,
    };
    int err;

    LOG_INF("Connecting to '%.*s'", wifi_ssid_len, wifi_ssid);

    wifi_mgmt_connecting_update(wifi_mgmt, true);

    err = net_mgmt(NET_REQUEST_WIFI_CONNECT, wifi_mgmt->iface, &params, sizeof(params));
    if (err == -EALREADY)
    {
        LOG_INF("already connected");

        wifi_mgmt_connecting_update(wifi_mgmt, false);

        if (atomic_test_and_clear_bit(&wifi_mgmt->flags, WIFI_FLAG_RECONNECT))
        {
            LOG_INF("reconnecting");

            err = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, wifi_mgmt->iface, NULL, 0);
            if (err)
            {
                LOG_ERR("failed to request disconnect: %d", err);
                goto handle_err;
            }
        }
        else
        {
            wifi_event_notify(wifi_mgmt, WIFI_EVENT_CONNECTED);
        }
    }
    else if (err)
    {
        LOG_ERR("failed to request connect: %d", err);
        goto handle_err;
    }

    return;

handle_err:
    wifi_mgmt_connecting_update(wifi_mgmt, false);
    wifi_event_notify(wifi_mgmt, WIFI_EVENT_DISCONNECTED);
}

static void wifi_disconnect(struct wifi_manager_data *wifi_mgmt)
{
    int err;

    err = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, wifi_mgmt->iface, NULL, 0);
    if (err)
    {
        LOG_ERR("failed to request disconnect: %d", err);
        wifi_event_notify(wifi_mgmt, WIFI_EVENT_DISCONNECTED);
    }
}

static void wifi_check_lower_up(struct wifi_manager_data *wifi_mgmt)
{
    if (IS_ENABLED(CONFIG_WIFI_ESP32) || net_if_flag_is_set(wifi_mgmt->iface, NET_IF_LOWER_UP))
    {
        wifi_event_notify(wifi_mgmt, WIFI_EVENT_LOWER_UP);
    }
    else
    {
        wifi_event_notify(wifi_mgmt, WIFI_EVENT_START);
    }
}

static void wifi_ready(struct wifi_manager_data *wifi_mgmt)
{
    LOG_DBG("ready");
}

static void wifi_state_change(struct wifi_manager_data *wifi_mgmt, enum wifi_state state)
{
    LOG_DBG("state %s (%d) -> %s (%d)",
            wifi_state_str(wifi_mgmt->state),
            wifi_mgmt->state,
            wifi_state_str(state),
            state);

    wifi_mgmt->state = state;
}

static void net_mgmt_event_handle(struct k_work *work)
{
    struct wifi_manager_data *wifi_mgmt =
        CONTAINER_OF(work, struct wifi_manager_data, net_mgmt_event_work);
    atomic_val_t net_state = atomic_get(&wifi_mgmt->net_state);

    if (wifi_mgmt->net_state_old == net_state)
    {
        /* Nothing to do */
        return;
    }

    if (net_state & NET_STATE_IPV4_READY)
    {
        wifi_event_notify(wifi_mgmt, WIFI_EVENT_IP_ADD);
    }
    else if (net_state & NET_STATE_WIFI_CONNECTED)
    {
        if (wifi_mgmt->net_state_old & NET_STATE_WIFI_CONNECTED)
        {
            wifi_event_notify(wifi_mgmt, WIFI_EVENT_IP_DEL);
        }
        else
        {
            wifi_event_notify(wifi_mgmt, WIFI_EVENT_CONNECTED);
        }
    }
    else
    {
        wifi_event_notify(wifi_mgmt, WIFI_EVENT_DISCONNECTED);
    }

    wifi_mgmt->net_state_old = net_state;
}

static void wifi_event_handle(struct k_work *work)
{
    struct wifi_manager_data *wifi_mgmt = CONTAINER_OF(work, struct wifi_manager_data, event_work);
    enum wifi_event event = wifi_mgmt->event;
    enum wifi_state old_state = wifi_mgmt->state;
    int map_value = wifi_state_change_map[event][old_state];
    enum wifi_state new_state = map_value - MAP_OFFSET;
    int sleep_msec = 0;

    if (map_value - MAP_OFFSET >= WIFI_STATE_WAIT)
    {
        new_state = old_state;
        sleep_msec = map_value - MAP_OFFSET - (int) WIFI_STATE_WAIT;
    }

    if (map_value == 0)
    {
        LOG_DBG("Nothing to do according to state map (state %s[%d])",
                wifi_state_str(old_state),
                (int) old_state);
        return;
    }

    LOG_DBG("event %s (%d) (%s[%d] -> %s[%d])",
            wifi_event_str(event),
            event,
            wifi_state_str(old_state),
            (int) old_state,
            wifi_state_str(new_state),
            (int) new_state);

    if (new_state == WIFI_STATE_INVALID)
    {
        LOG_ERR("Invalid event %s (%d) during state %s (%d)",
                wifi_event_str(event),
                event,
                wifi_state_str(old_state),
                old_state);
        return;
    }

    wifi_state_change(wifi_mgmt, new_state);

    if (new_state == old_state)
    {
        /* Give a bit of time between retries */
        LOG_DBG("Sleeping for %d ms", sleep_msec);
        k_sleep(K_MSEC(sleep_msec));
    }

    switch (new_state)
    {
        case WIFI_STATE_IDLE:
            break;
        case WIFI_STATE_WAITING_FOR_LOWER_UP:
            wifi_check_lower_up(wifi_mgmt);
            break;
        case WIFI_STATE_CONNECTING:
            wifi_connect(wifi_mgmt);
            break;
        case WIFI_STATE_DISCONNECTING:
            wifi_disconnect(wifi_mgmt);
            break;
        case WIFI_STATE_READY:
            wifi_ready(wifi_mgmt);
            break;
        case WIFI_STATE_INVALID:
        case WIFI_STATE_WAIT:
            break;
    }
}

static inline atomic_val_t atomic_update(atomic_t *target, atomic_val_t value, atomic_val_t mask)
{
    atomic_val_t flags;

    do
    {
        flags = atomic_get(target);
    } while (!atomic_cas(target, flags, (flags & ~mask) | value));

    return flags;
}

static void ipv4_changed(struct net_mgmt_event_callback *cb,
                         uint32_t mgmt_event,
                         struct net_if *iface)
{
    struct wifi_manager_data *wifi_mgmt = CONTAINER_OF(cb, struct wifi_manager_data, ipv4_mgmt_cb);

    LOG_DBG("ipv4 event: %x", (unsigned int) mgmt_event);

    switch (mgmt_event)
    {
        case NET_EVENT_IPV4_ADDR_ADD:
            atomic_or(&wifi_mgmt->net_state, NET_STATE_IPV4_READY);
            k_work_submit_to_queue(&wifi_mgmt->workq, &wifi_mgmt->net_mgmt_event_work);
            break;
        case NET_EVENT_IPV4_ADDR_DEL:
            atomic_and(&wifi_mgmt->net_state, ~NET_STATE_IPV4_READY);
            k_work_submit_to_queue(&wifi_mgmt->workq, &wifi_mgmt->net_mgmt_event_work);
            break;
    }
}

static inline int wifi_connect_error(const struct wifi_status *status)
{
    return status->status;
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                    uint32_t mgmt_event,
                                    struct net_if *iface)
{
    struct wifi_manager_data *wifi_mgmt = CONTAINER_OF(cb, struct wifi_manager_data, wifi_mgmt_cb);
    const struct wifi_status *status = cb->info;

    LOG_DBG("wifi event: %x", (unsigned int) mgmt_event);

    switch (mgmt_event)
    {
        case NET_EVENT_WIFI_CONNECT_RESULT:
            if (wifi_connect_error(status))
            {
                atomic_and(
                    &wifi_mgmt->net_state,
                    ~(NET_STATE_WIFI_CONNECTED | NET_STATE_WIFI_CONNECTING | NET_STATE_IPV4_READY));
            }
            else
            {
                atomic_update(&wifi_mgmt->net_state,
                              NET_STATE_WIFI_CONNECTED,
                              (NET_STATE_WIFI_CONNECTED | NET_STATE_WIFI_CONNECTING));
            }
            k_work_submit_to_queue(&wifi_mgmt->workq, &wifi_mgmt->net_mgmt_event_work);
            break;
        case NET_EVENT_WIFI_DISCONNECT_RESULT:
            atomic_and(
                &wifi_mgmt->net_state,
                ~(NET_STATE_WIFI_CONNECTED | NET_STATE_WIFI_CONNECTING | NET_STATE_IPV4_READY));
            k_work_submit_to_queue(&wifi_mgmt->workq, &wifi_mgmt->net_mgmt_event_work);
            break;
        default:
            break;
    }
}

static struct wifi_manager_data wifi_manager_data;

K_THREAD_STACK_DEFINE(wifi_manager_work_q_stack, CONFIG_GOLIOTH_SAMPLE_WIFI_STACK_SIZE);

static const struct wifi_manager_config wifi_manager_config = {
    .workq_stack = wifi_manager_work_q_stack,
    .workq_stack_size = K_THREAD_STACK_SIZEOF(wifi_manager_work_q_stack),
    .workq_config =
        {
            .name = "wifi_manager",
        },
};

static int wifi_manager_init(void)
{
    struct wifi_manager_data *wifi_mgmt = &wifi_manager_data;
    const struct wifi_manager_config *config = &wifi_manager_config;

    wifi_mgmt->iface = net_if_get_first_wifi();
    __ASSERT_NO_MSG(wifi_mgmt->iface);

    k_work_queue_start(&wifi_mgmt->workq,
                       config->workq_stack,
                       config->workq_stack_size,
                       CONFIG_GOLIOTH_SAMPLE_WIFI_THREAD_PRIORITY,
                       &config->workq_config);
    k_work_init(&wifi_mgmt->event_work, wifi_event_handle);
    k_work_init(&wifi_mgmt->net_mgmt_event_work, net_mgmt_event_handle);

    net_mgmt_init_event_callback(&wifi_mgmt->ipv4_mgmt_cb,
                                 ipv4_changed,
                                 NET_EVENT_IPV4_ADDR_ADD | NET_EVENT_IPV4_ADDR_DEL);
    net_mgmt_add_event_callback(&wifi_mgmt->ipv4_mgmt_cb);

    net_mgmt_init_event_callback(&wifi_mgmt->wifi_mgmt_cb,
                                 wifi_mgmt_event_handler,
                                 WIFI_MANAGER_MGMT_EVENTS);
    net_mgmt_add_event_callback(&wifi_mgmt->wifi_mgmt_cb);

    wifi_event_notify(wifi_mgmt, WIFI_EVENT_START);
    k_work_submit_to_queue(&wifi_mgmt->workq, &wifi_mgmt->event_work);

    return 0;
}

SYS_INIT(wifi_manager_init, APPLICATION, CONFIG_GOLIOTH_SAMPLE_WIFI_INIT_PRIORITY);
