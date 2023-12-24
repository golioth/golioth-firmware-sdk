#include <golioth/golioth.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define TAG "main"

#define MAX_DER_FILE_SIZE 2048

static int read_file(const char* filepath, uint8_t* buffer, size_t buffer_size) {
    FILE* fp = fopen(filepath, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open file: %s\n", filepath);
        return -1;
    }
    size_t bytes_read = fread(buffer, 1, buffer_size, fp);
    if (bytes_read <= 0) {
        fprintf(stderr, "Failed to read file: %s\n", filepath);
    }
    return bytes_read;
}

int main(void) {
    uint8_t client_key_der[MAX_DER_FILE_SIZE];
    uint8_t client_cert_der[MAX_DER_FILE_SIZE];
    uint8_t root_ca_der[MAX_DER_FILE_SIZE];

    size_t client_key_len = read_file("certs/client.key.der", client_key_der, sizeof(client_key_der));
    size_t client_cert_len =
            read_file("certs/client.crt.der", client_cert_der, sizeof(client_cert_der));
    size_t root_ca_len = read_file("isrgrootx1.der", root_ca_der, sizeof(root_ca_der));
    if ((client_key_len <= 0) || (client_cert_len <= 0) || (root_ca_len <= 0)) {
        return 1;
    }

    golioth_client_config_t config = {
            .credentials = {
                    .auth_type = GOLIOTH_TLS_AUTH_TYPE_PKI,
                    .pki = {
                            .ca_cert = root_ca_der,
                            .ca_cert_len = root_ca_len,
                            .public_cert = client_cert_der,
                            .public_cert_len = client_cert_len,
                            .private_key = client_key_der,
                            .private_key_len = client_key_len,
                    }}};

    golioth_client_t client = golioth_client_create(&config);
    assert(client);

    bool connected = golioth_client_wait_for_connect(client, 1000);
    golioth_client_stop(client);

    if (!connected) {
        fprintf(stderr, "Failed to connect\n");
        return 2;
    }

    return 0;
}
