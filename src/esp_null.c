#include "esp_null.h"

#include <string.h>

static void write32(unsigned char *out, uint32_t n)
{
    out[0] = (unsigned char)(n >> 24); out[1] = (unsigned char)(n >> 16);
    out[2] = (unsigned char)(n >> 8); out[3] = (unsigned char)n;
}

static uint32_t read32(const unsigned char *in)
{
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
           ((uint32_t)in[2] << 8) | in[3];
}

int esp_null_encode(const struct esp_null_sa *sa, uint32_t sequence,
                    const void *payload, size_t payload_length, unsigned char next_header,
                    unsigned char *packet, size_t *packet_length)
{
    size_t padding = (4 - ((payload_length + 2) % 4)) % 4, i;
    size_t body_length = 8 + payload_length + padding + 2;
    if (!sequence || payload_length > 65535 || *packet_length < body_length + 12) return -1;
    write32(packet, sa->spi); write32(packet + 4, sequence);
    if (packet + 8 != payload) memcpy(packet + 8, payload, payload_length);
    for (i = 0; i < padding; i++) packet[8 + payload_length + i] = (unsigned char)(i + 1);
    packet[body_length - 2] = (unsigned char)padding;
    packet[body_length - 1] = next_header;
    if (ims_hmac_96(sa->integrity, sa->key, sa->key_length, packet, body_length,
                    packet + body_length)) return -1;
    *packet_length = body_length + 12;
    return 0;
}

int esp_null_decode(struct esp_null_sa *sa, const unsigned char *packet, size_t packet_length,
                    const unsigned char **payload, size_t *payload_length, unsigned char *next_header)
{
    unsigned char digest[12];
    unsigned int difference = 0, padding;
    uint32_t sequence, delta;
    size_t body_length, i;
    if (packet_length < 24 || read32(packet) != sa->spi) return -1;
    body_length = packet_length - 12;
    if ((body_length - 8) % 4) return -1;
    sequence = read32(packet + 4);
    if (!sequence) return -1;
    if (sequence <= sa->highest_sequence) {
        delta = sa->highest_sequence - sequence;
        if (delta >= 64 || (sa->replay_window & ((uint64_t)1 << delta))) return -1;
    }
    if (ims_hmac_96(sa->integrity, sa->key, sa->key_length, packet, body_length, digest)) return -1;
    for (i = 0; i < sizeof(digest); i++) difference |= digest[i] ^ packet[body_length + i];
    if (difference) return -1;
    padding = packet[body_length - 2];
    if (padding > body_length - 10) return -1;
    for (i = 0; i < padding; i++)
        if (packet[body_length - 2 - padding + i] != i + 1) return -1;
    *payload = packet + 8; *payload_length = body_length - 10 - padding;
    *next_header = packet[body_length - 1];
    if (sequence > sa->highest_sequence) {
        delta = sequence - sa->highest_sequence;
        sa->replay_window = delta >= 64 ? 1 : (sa->replay_window << delta) | 1;
        sa->highest_sequence = sequence;
    } else sa->replay_window |= (uint64_t)1 << (sa->highest_sequence - sequence);
    return 0;
}
