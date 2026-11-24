#include "udp6.h"

#include <string.h>

static uint32_t sum_bytes(uint32_t sum, const unsigned char *data, size_t length)
{
    while (length >= 2) {
        sum += ((uint32_t)data[0] << 8) | data[1];
        data += 2; length -= 2;
    }
    if (length) sum += (uint32_t)*data << 8;
    return sum;
}

static uint16_t checksum(const unsigned char source[16], const unsigned char destination[16],
                         const unsigned char *packet, size_t length)
{
    uint32_t sum = sum_bytes(0, source, 16);
    sum = sum_bytes(sum, destination, 16);
    sum += (uint32_t)(length >> 16) + (uint32_t)(length & 0xffff) + 17;
    sum = sum_bytes(sum, packet, length);
    while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
    return (uint16_t)~sum;
}

int udp6_encode(const unsigned char source[16], const unsigned char destination[16],
                uint16_t source_port, uint16_t destination_port,
                const void *payload, size_t payload_length,
                unsigned char *packet, size_t *packet_length)
{
    size_t n = payload_length + 8;
    uint16_t value;
    if (payload_length > 65527 || *packet_length < n) return -1;
    packet[0] = (unsigned char)(source_port >> 8); packet[1] = (unsigned char)source_port;
    packet[2] = (unsigned char)(destination_port >> 8); packet[3] = (unsigned char)destination_port;
    packet[4] = (unsigned char)(n >> 8); packet[5] = (unsigned char)n;
    packet[6] = packet[7] = 0;
    memcpy(packet + 8, payload, payload_length);
    value = checksum(source, destination, packet, n);
    if (!value) value = 0xffff;
    packet[6] = (unsigned char)(value >> 8); packet[7] = (unsigned char)value;
    *packet_length = n;
    return 0;
}

int udp6_decode(const unsigned char source[16], const unsigned char destination[16],
                const unsigned char *packet, size_t packet_length,
                uint16_t *source_port, uint16_t *destination_port,
                const unsigned char **payload, size_t *payload_length)
{
    size_t n;
    if (packet_length < 8) return -1;
    n = ((size_t)packet[4] << 8) | packet[5];
    if (n != packet_length || (!packet[6] && !packet[7]) || checksum(source, destination, packet, n)) return -1;
    *source_port = (uint16_t)(((unsigned int)packet[0] << 8) | packet[1]);
    *destination_port = (uint16_t)(((unsigned int)packet[2] << 8) | packet[3]);
    *payload = packet + 8; *payload_length = n - 8;
    return 0;
}
