#include "golioth.h"
#include "golioth_basics.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#define TAG "main"

int main(void) {
    char* golioth_psk_id = getenv("GOLIOTH_SAMPLE_PSK_ID");
    if ((!golioth_psk_id) || strlen(golioth_psk_id) <= 0) {
        fprintf(stderr, "PSK ID is not specified.\n");
        return 1;
    }
    char* golioth_psk = getenv("GOLIOTH_SAMPLE_PSK");
    if ((!golioth_psk) || strlen(golioth_psk) <= 0) {
        fprintf(stderr, "PSK is not specified.\n");
        return 1;
    }

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
