#include <fff.h>

#include <coap_client.h>

DECLARE_FAKE_VALUE_FUNC(enum golioth_status,
                        golioth_coap_client_observe,
                        struct golioth_client *,
                        const char *,
                        const char *,
                        uint32_t,
                        golioth_get_cb_fn,
                        void *);
DECLARE_FAKE_VALUE_FUNC(enum golioth_status,
                        golioth_coap_client_set,
                        struct golioth_client *,
                        const char *,
                        const char *,
                        uint32_t,
                        const uint8_t *,
                        size_t,
                        golioth_set_cb_fn,
                        void *,
                        bool,
                        int32_t);
