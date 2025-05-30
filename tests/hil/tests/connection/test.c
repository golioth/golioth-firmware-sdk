#include <golioth/client.h>
#include <golioth/golioth_sys.h>

LOG_TAG_DEFINE(test_connect);

static golioth_sys_sem_t _connected_sem = NULL;

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected)
    {
        golioth_sys_sem_give(_connected_sem);
    }
    GLTH_LOGI(TAG, "Golioth client %s", is_connected ? "connected" : "disconnected");
}

void hil_test_entry(const struct golioth_client_config *config)
{
    _connected_sem = golioth_sys_sem_create(1, 0);

    struct golioth_client *client = golioth_client_create(config);

    golioth_client_register_event_callback(client, on_client_event, NULL);
    golioth_sys_sem_take(_connected_sem, GOLIOTH_SYS_WAIT_FOREVER);

    /* Pause to ensure we don't have out-of-order logs */
    golioth_sys_msleep(1 * 1000);

    GLTH_LOGI(TAG, "Stopping client");
    golioth_client_stop(client);

    golioth_sys_msleep(10 * 1000);

    GLTH_LOGI(TAG, "Starting client");
    golioth_client_start(client);

    golioth_sys_sem_take(_connected_sem, GOLIOTH_SYS_WAIT_FOREVER);

    /* Pause to ensure we don't have out-of-order logs */
    golioth_sys_msleep(1 * 1000);

    GLTH_LOGI(TAG, "Destroying client");
    golioth_client_destroy(client);
    client = NULL;

    golioth_sys_msleep(10 * 1000);

    GLTH_LOGI(TAG, "Starting client");
    client = golioth_client_create(config);
    golioth_client_start(client);

    golioth_sys_sem_take(_connected_sem, GOLIOTH_SYS_WAIT_FOREVER);

    /* Ensure keepalive timer is running */

    golioth_sys_msleep(60 * 1000);

    /* Pause to ensure logs are show */
    golioth_sys_msleep(1 * 1000);
}
