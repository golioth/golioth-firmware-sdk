#include <fff.h>
#include "coap_client_fake.h"

DEFINE_FAKE_VALUE_FUNC(enum golioth_status,
                       golioth_coap_client_observe,
                       struct golioth_client *,
                       const char *,
                       const char *,
                       uint32_t,
                       golioth_get_cb_fn,
                       void *);
DEFINE_FAKE_VALUE_FUNC(enum golioth_status,
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
DEFINE_FAKE_VOID_FUNC(golioth_coap_client_cancel_observations_by_prefix,
                      struct golioth_client *,
                      const char *);
