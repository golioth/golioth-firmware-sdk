#include "golioth.h"
#include "golioth_basics.h"
#include <string.h>
#include <assert.h>

#define TAG "main"

// Include user's Golioth credentials.
//
// This is a required file, so if it doesn't exist, you will
// need to create it and add the following:
//
//    #define GOLIOTH_PSK_ID "device@project"
//    #define GOLIOTH_PSK "secret"
//
// The Golioth credentials can be found at:
//
//    console.golioth.io -> Devices -> (yourdevice) -> Credentials
#include "credentials.inc"

int main(void) {
    golioth_client_config_t config = {
            .credentials = {
                    .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
                    .psk = {
                            .psk_id = GOLIOTH_PSK_ID,
                            .psk_id_len = strlen(GOLIOTH_PSK_ID),
                            .psk = GOLIOTH_PSK,
                            .psk_len = strlen(GOLIOTH_PSK),
                    }}};

    golioth_client_t client = golioth_client_create(&config);
    assert(client);
    golioth_basics(client);

    return 0;
}
