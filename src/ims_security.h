#ifndef IMS_SECURITY_H
#define IMS_SECURITY_H

#include "ims_digest.h"
#include "sip_message.h"
#include <stdint.h>

struct ims_security_parameters {
    enum ims_integrity integrity;
    uint32_t spi_client, spi_server;
    uint16_t port_client, port_server;
};

/* Returns 0 for a supported NULL-encryption ESP offer, -1 otherwise. */
int ims_security_server(const struct sip_slice *header,
                        const struct ims_security_parameters *local,
                        struct ims_security_parameters *selected);

int ims_security_client(const struct ims_security_parameters *local,
                        char *output, size_t capacity);

#endif
