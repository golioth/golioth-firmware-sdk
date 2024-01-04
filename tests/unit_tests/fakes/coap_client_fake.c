#include <fff.h>
#include "coap_client_fake.h"

DEFINE_FAKE_VALUE_FUNC(
        enum golioth_status,
        golioth_coap_client_observe_async,
        struct golioth_client*,
        const char*,
        const char*,
        uint32_t,
        golioth_get_cb_fn,
        void*);
DEFINE_FAKE_VALUE_FUNC(
        enum golioth_status,
        golioth_coap_client_set,
        struct golioth_client*,
        const char*,
        const char*,
        uint32_t,
        const uint8_t*,
        size_t,
        golioth_set_cb_fn,
        void*,
        bool,
        int32_t);
