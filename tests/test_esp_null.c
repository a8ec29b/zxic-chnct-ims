#include "esp_null.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const unsigned char expected[] = {
        0x01,0x02,0x03,0x04,0,0,0,1,0x61,0x62,0x63,1,2,3,3,17,
        0xba,0xf6,0x70,0x15,0x07,0x66,0xcd,0x9a,0xa6,0x56,0x4e,0x24
    };
    unsigned char key[20], packet[128], next;
    const unsigned char *payload;
    size_t i, length = sizeof(packet), payload_length;
    struct esp_null_sa sa = {0x01020304, IMS_HMAC_SHA1_96, key, sizeof(key), 0, 0};
    for (i = 0; i < sizeof(key); i++) key[i] = (unsigned char)i;
    assert(esp_null_encode(&sa, 1, "abc", 3, 17, packet, &length) == 0);
    assert(length == sizeof(expected) && !memcmp(packet, expected, length));
    packet[10] ^= 1;
    assert(esp_null_decode(&sa, packet, length, &payload, &payload_length, &next) == -1);
    assert(sa.highest_sequence == 0);
    packet[10] ^= 1;
    assert(esp_null_decode(&sa, packet, length, &payload, &payload_length, &next) == 0);
    assert(payload_length == 3 && !memcmp(payload, "abc", 3) && next == 17);
    assert(esp_null_decode(&sa, packet, length, &payload, &payload_length, &next) == -1);
    length = sizeof(packet);
    assert(esp_null_encode(&sa, 3, "x", 1, 17, packet, &length) == 0);
    assert(esp_null_decode(&sa, packet, length, &payload, &payload_length, &next) == 0);
    length = sizeof(packet);
    assert(esp_null_encode(&sa, 2, "y", 1, 17, packet, &length) == 0);
    assert(esp_null_decode(&sa, packet, length, &payload, &payload_length, &next) == 0);
    assert(esp_null_decode(&sa, packet, length, &payload, &payload_length, &next) == -1);
    puts("esp_null: ok");
    return 0;
}
