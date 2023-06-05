#include <fff.h>

#include "golioth_time_fake.h"

DEFINE_FAKE_VALUE_FUNC(uint64_t, golioth_time_millis);
