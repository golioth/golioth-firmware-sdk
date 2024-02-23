#include <golioth/client.h>
#include <golioth/golioth_debug.h>
#include <golioth/golioth_sys.h>
#include <golioth/settings.h>

LOG_TAG_DEFINE(test_settings);

static enum golioth_settings_status on_test_int(int32_t new_value, void *arg)
{
    GLTH_LOGI(TAG, "Received test_int: %" PRId32, new_value);

    return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_test_int_range(int32_t new_value, void *arg)
{
    GLTH_LOGI(TAG, "Received test_int_range: %" PRId32, new_value);

    return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_test_bool(bool new_value, void *arg)
{
    GLTH_LOGI(TAG, "Received test_bool: %s", new_value ? "true" : "false");

    return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_test_float(float new_value, void *arg)
{
    GLTH_LOGI(TAG, "Received test_float: %f", (double) new_value);

    return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_test_string(const char *new_value,
                                                   size_t new_value_len,
                                                   void *arg)
{
    GLTH_LOGI(TAG, "Received test_string: %.*s", new_value_len, new_value);

    return GOLIOTH_SETTINGS_SUCCESS;
}

void hil_test_entry(const struct golioth_client_config *config)
{
    enum golioth_status status;

    golioth_debug_set_cloud_log_enabled(false);

    struct golioth_client *client = golioth_client_create(config);

    struct golioth_settings *settings = golioth_settings_init(client);

    status = golioth_settings_register_int(settings, "TEST_INT", on_test_int, NULL);

    status = golioth_settings_register_int_with_range(settings,
                                                      "TEST_INT_RANGE",
                                                      0,
                                                      100,
                                                      on_test_int_range,
                                                      NULL);

    status = golioth_settings_register_bool(settings, "TEST_BOOL", on_test_bool, NULL);

    status = golioth_settings_register_float(settings, "TEST_FLOAT", on_test_float, NULL);

    status = golioth_settings_register_string(settings, "TEST_STRING", on_test_string, NULL);

    status = golioth_settings_register_int(settings, "TEST_WRONG_TYPE", on_test_int, NULL);

    (void) status;

    while (1)
    {
        golioth_sys_msleep(5000);
    }
}
