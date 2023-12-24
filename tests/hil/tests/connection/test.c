#include <golioth/golioth.h>

LOG_TAG_DEFINE(test_connect);

void hil_test_entry(const golioth_client_config_t *config)
{
    golioth_client_t client = golioth_client_create(config);

    (void) client;

    while(1)
    {
        golioth_sys_msleep(5000);
    }
}
