#ifndef UDP6_H
#define UDP6_H

#include <stddef.h>
#include <stdint.h>

int udp6_encode(const unsigned char source[16], const unsigned char destination[16],
                uint16_t source_port, uint16_t destination_port,
                const void *payload, size_t payload_length,
                unsigned char *packet, size_t *packet_length);
int udp6_decode(const unsigned char source[16], const unsigned char destination[16],
                const unsigned char *packet, size_t packet_length,
                uint16_t *source_port, uint16_t *destination_port,
                const unsigned char **payload, size_t *payload_length);

#endif
