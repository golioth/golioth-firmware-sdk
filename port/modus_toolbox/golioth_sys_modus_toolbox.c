#include <golioth/golioth_sys.h>
#include <golioth/golioth_status.h>
#include "mbedtls/sha256.h"
#include "../utils/hex.h"

/*-------------------------------------------------
 * Hash
 *------------------------------------------------*/

golioth_sys_sha256_t golioth_sys_sha256_create(void)
{
    mbedtls_sha256_context *hash = golioth_sys_malloc(sizeof(mbedtls_sha256_context));
    if (!hash)
    {
        return NULL;
    }

    mbedtls_sha256_init(hash);
    mbedtls_sha256_starts_ret(hash, 0);

    return (golioth_sys_sha256_t) hash;
}

void golioth_sys_sha256_destroy(golioth_sys_sha256_t sha_ctx)
{
    if (!sha_ctx)
    {
        return;
    }

    mbedtls_sha256_context *hash = sha_ctx;
    mbedtls_sha256_free(hash);
}

enum golioth_status golioth_sys_sha256_update(golioth_sys_sha256_t sha_ctx,
                                              const uint8_t *input,
                                              size_t len)
{
    if (!sha_ctx || !input)
    {
        return GOLIOTH_ERR_NULL;
    }

    mbedtls_sha256_context *hash = sha_ctx;
    int err = mbedtls_sha256_update_ret(hash, input, len);
    if (err)
    {
        return GOLIOTH_ERR_FAIL;
    }

    return GOLIOTH_OK;
}

enum golioth_status golioth_sys_sha256_finish(golioth_sys_sha256_t sha_ctx, uint8_t *output)
{
    if (!sha_ctx || !output)
    {
        return GOLIOTH_ERR_NULL;
    }

    mbedtls_sha256_context *hash = sha_ctx;
    int err = mbedtls_sha256_finish_ret(hash, output);
    if (err)
    {
        return GOLIOTH_ERR_FAIL;
    }

    return GOLIOTH_OK;
}

size_t golioth_sys_hex2bin(const char *hex, size_t hexlen, uint8_t *buf, size_t buflen)
{
    return hex2bin(hex, hexlen, buf, buflen);
}
