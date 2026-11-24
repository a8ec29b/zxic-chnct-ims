#ifndef IMS_DIGEST_H
#define IMS_DIGEST_H

#include <stddef.h>
#include <stdint.h>

enum ims_integrity { IMS_HMAC_MD5_96, IMS_HMAC_SHA1_96 };

int ims_md5(const void *data, size_t length, unsigned char digest[16]);
int ims_hmac_96(enum ims_integrity algorithm, const void *key, size_t key_length,
                const void *data, size_t length, unsigned char digest[12]);

/* AKAv1-MD5 uses the binary USIM RES as the digest password. */
int ims_aka_v1_digest(const char *username, const char *realm, const char *uri,
                      const char *nonce, const unsigned char *res, size_t res_length,
                      const char *cnonce, uint32_t nonce_count, char response[33]);

#endif
