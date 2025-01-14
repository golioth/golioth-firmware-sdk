#include <fff.h>

#include <coap_client.h>

DECLARE_FAKE_VALUE_FUNC(enum golioth_status,
                        golioth_coap_client_observe,
                        struct golioth_client *,
                        const uint8_t *,
                        const char *,
                        const char *,
                        uint32_t,
                        golioth_get_cb_fn,
                        void *);
DECLARE_FAKE_VALUE_FUNC(enum golioth_status,
                        golioth_coap_client_set,
                        struct golioth_client *,
                        const uint8_t *,
                        const char *,
                        const char *,
                        uint32_t,
                        const uint8_t *,
                        size_t,
                        golioth_set_cb_fn,
                        void *,
                        bool,
                        int32_t);
DECLARE_FAKE_VOID_FUNC(golioth_coap_client_cancel_observations_by_prefix,
                       struct golioth_client *,
                       const char *);
DECLARE_FAKE_VOID_FUNC(golioth_coap_next_token, uint8_t *);
