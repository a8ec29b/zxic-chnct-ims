#ifndef IMS_TRANSPORT_H
#define IMS_TRANSPORT_H

#include "esp_null.h"
#include "ims_security.h"
#include <stddef.h>
#include <stdint.h>

enum ims_flow { IMS_FLOW_CLIENT, IMS_FLOW_SERVER };

struct ims_transport_flow {
    struct esp_null_sa incoming, outgoing;
    uint16_t local_port, remote_port;
    uint32_t next_sequence;
};

struct ims_transport {
    unsigned char local_address[16], remote_address[16], integrity_key[16];
    struct ims_transport_flow flows[2];
};

/* The transport owns a copy of IK; discard it when the registration ends. */
void ims_transport_init(struct ims_transport *t, const unsigned char local[16],
                        const unsigned char remote[16], const unsigned char ik[16],
                        const struct ims_security_parameters *client,
                        const struct ims_security_parameters *server);
int ims_transport_encode(struct ims_transport *t, enum ims_flow flow,
                         const void *sip, size_t sip_length,
                         unsigned char *packet, size_t *packet_length);
/* SIP points into packet and remains valid while the caller retains it. */
int ims_transport_decode(struct ims_transport *t, const unsigned char *packet, size_t length,
                         enum ims_flow *flow, const unsigned char **sip, size_t *sip_length);

#endif
