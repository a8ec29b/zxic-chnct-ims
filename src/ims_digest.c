#include "ims_digest.h"

#include "mbedtls/md.h"
#include "mbedtls/md5.h"
#include "mbedtls/platform_util.h"
#include <stdio.h>
#include <string.h>

int ims_md5(const void *data, size_t length, unsigned char digest[16])
{
    return mbedtls_md5(data, length, digest);
}

int ims_hmac_96(enum ims_integrity algorithm, const void *key, size_t key_length,
                const void *data, size_t length, unsigned char digest[12])
{
    unsigned char full[20];
    const mbedtls_md_info_t *info;
    int rc;
    if (algorithm == IMS_HMAC_MD5_96) info = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
    else if (algorithm == IMS_HMAC_SHA1_96) info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    else return -1;
    rc = mbedtls_md_hmac(info, key, key_length, data, length, full);
    if (!rc) memcpy(digest, full, 12);
    mbedtls_platform_zeroize(full, sizeof(full));
    return rc;
}

static void md5_text(mbedtls_md5_context *ctx, const char *text)
{
    mbedtls_md5_update(ctx, (const unsigned char *)text, strlen(text));
}

static void hex16(const unsigned char digest[16], char output[33])
{
    static const char digits[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < 16; i++) {
        output[i * 2] = digits[digest[i] >> 4];
        output[i * 2 + 1] = digits[digest[i] & 15];
    }
    output[32] = 0;
}

int ims_aka_v1_digest(const char *username, const char *realm, const char *uri,
                      const char *nonce, const unsigned char *res, size_t res_length,
                      const char *cnonce, uint32_t nonce_count, char response[33])
{
    mbedtls_md5_context ctx;
    unsigned char raw[16];
    char h1[33], h2[33], nc[9];
    if (!res_length || (cnonce && (!*cnonce || !nonce_count))) return -1;
    mbedtls_md5_init(&ctx);
    mbedtls_md5_starts(&ctx);
    md5_text(&ctx, username); md5_text(&ctx, ":"); md5_text(&ctx, realm); md5_text(&ctx, ":");
    mbedtls_md5_update(&ctx, res, res_length);
    mbedtls_md5_finish(&ctx, raw); hex16(raw, h1);
    mbedtls_md5_starts(&ctx);
    md5_text(&ctx, "REGISTER:"); md5_text(&ctx, uri);
    mbedtls_md5_finish(&ctx, raw); hex16(raw, h2);
    mbedtls_md5_starts(&ctx);
    md5_text(&ctx, h1); md5_text(&ctx, ":"); md5_text(&ctx, nonce); md5_text(&ctx, ":");
    if (cnonce) {
        snprintf(nc, sizeof(nc), "%08x", (unsigned int)nonce_count);
        md5_text(&ctx, nc); md5_text(&ctx, ":"); md5_text(&ctx, cnonce); md5_text(&ctx, ":auth:");
    }
    md5_text(&ctx, h2); mbedtls_md5_finish(&ctx, raw); hex16(raw, response);
    mbedtls_md5_free(&ctx);
    mbedtls_platform_zeroize(raw, sizeof(raw));
    mbedtls_platform_zeroize(h1, sizeof(h1));
    return 0;
}
