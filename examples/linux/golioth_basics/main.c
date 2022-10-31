#include "golioth.h"
#include "golioth_basics.h"
#include <string.h>
#include <assert.h>

#define TAG "main"

int main(void) {
    const char* golioth_psk_id = "20221028174758-laptop@nicks-first-project";
    const char* golioth_psk = "c3e1a88504de7b53c4e5cce1c28887ad";

    golioth_client_config_t config = {
            .credentials = {
                    .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
                    .psk = {
                            .psk_id = golioth_psk_id,
                            .psk_id_len = strlen(golioth_psk_id),
                            .psk = golioth_psk,
                            .psk_len = strlen(golioth_psk),
                    }}};

    golioth_client_t client = golioth_client_create(&config);
    assert(client);
    golioth_basics(client);

    return 0;
}
