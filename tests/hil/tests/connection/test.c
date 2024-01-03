#include <golioth/client.h>
#include <golioth/golioth_sys.h>

LOG_TAG_DEFINE(test_connect);

void hil_test_entry(const struct golioth_client_config *config)
{
    struct golioth_client* client = golioth_client_create(config);

    (void) client;

    while(1)
    {
        golioth_sys_msleep(5000);
    }
}
