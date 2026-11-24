#include "ims_digest.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const unsigned char md5_expected[16] = {
        0x90,0x01,0x50,0x98,0x3c,0xd2,0x4f,0xb0,0xd6,0x96,0x3f,0x7d,0x28,0xe1,0x7f,0x72
    };
    static const unsigned char hmac_md5[12] = {
        0x80,0x07,0x07,0x13,0x46,0x3e,0x77,0x49,0xb9,0x0c,0x2d,0xc2
    };
    static const unsigned char hmac_sha1[12] = {
        0xde,0x7c,0x9b,0x85,0xb8,0xb7,0x8a,0xa6,0xbc,0x8a,0x7a,0x36
    };
    static const char text[] = "The quick brown fox jumps over the lazy dog";
    static const unsigned char res[] = {0,0x11,0x22,0xff,0x33,0x44,0x55,0x66};
    unsigned char result[16];
    char response[33];
    assert(ims_md5("abc", 3, result) == 0 && !memcmp(result, md5_expected, 16));
    assert(ims_hmac_96(IMS_HMAC_MD5_96, "key", 3, text, strlen(text), result) == 0);
    assert(!memcmp(result, hmac_md5, 12));
    assert(ims_hmac_96(IMS_HMAC_SHA1_96, "key", 3, text, strlen(text), result) == 0);
    assert(!memcmp(result, hmac_sha1, 12));
    assert(ims_aka_v1_digest("460110123456789@ims.mnc011.mcc460.3gppnetwork.org",
        "ims.mnc011.mcc460.3gppnetwork.org", "sip:ims.mnc011.mcc460.3gppnetwork.org",
        "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=", res, sizeof(res),
        "cafebabe", 1, response) == 0);
    assert(!strcmp(response, "fc3b00abb7fd9055c7287f3ff3bc9c64"));
    puts("ims_digest: ok");
    return 0;
}
