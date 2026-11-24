#include "udp6.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const unsigned char src[16] = {0x20,1,0x0d,0xb8,0,0,0,0,0,0,0,0,0,0,0,1};
    static const unsigned char dst[16] = {0x20,1,0x0d,0xb8,0,0,0,0,0,0,0,0,0,0,0,2};
    unsigned char packet[64];
    const unsigned char *payload;
    uint16_t sport, dport;
    size_t len = sizeof(packet), payload_len;
    assert(udp6_encode(src, dst, 5060, 5061, "abc", 3, packet, &len) == 0);
    assert(len == 11 && packet[0] == 0x13 && packet[1] == 0xc4);
    assert(udp6_decode(src, dst, packet, len, &sport, &dport, &payload, &payload_len) == 0);
    assert(sport == 5060 && dport == 5061 && payload_len == 3 && !memcmp(payload, "abc", 3));
    packet[8] ^= 1;
    assert(udp6_decode(src, dst, packet, len, &sport, &dport, &payload, &payload_len) == -1);
    puts("udp6: ok");
    return 0;
}
