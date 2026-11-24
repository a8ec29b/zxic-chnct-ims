#ifndef ESP_NULL_H
#define ESP_NULL_H

#include "ims_digest.h"
#include <stddef.h>
#include <stdint.h>

struct esp_null_sa {
    uint32_t spi;
    enum ims_integrity integrity;
    const unsigned char *key;
    size_t key_length;
    uint32_t highest_sequence;
    uint64_t replay_window;
};

/* ESP transport mode with the NULL encryption algorithm and 96-bit ICV. */
int esp_null_encode(const struct esp_null_sa *sa, uint32_t sequence,
                    const void *payload, size_t payload_length, unsigned char next_header,
                    unsigned char *packet, size_t *packet_length);
int esp_null_decode(struct esp_null_sa *sa, const unsigned char *packet, size_t packet_length,
                    const unsigned char **payload, size_t *payload_length, unsigned char *next_header);

#endif
