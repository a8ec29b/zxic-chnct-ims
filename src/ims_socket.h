#ifndef IMS_SOCKET_H
#define IMS_SOCKET_H

#include "ims_transport.h"

/* Opens an IPv6 ESP socket bound to the IMS PDN address. */
int ims_socket_open(const struct ims_transport *transport, const char *interface);
int ims_socket_send(int fd, struct ims_transport *transport, enum ims_flow flow,
                    const void *sip, size_t sip_length);
/* Receives one ESP datagram; sip points into the supplied packet buffer. */
int ims_socket_receive(int fd, struct ims_transport *transport,
                       unsigned char *packet, size_t capacity, enum ims_flow *flow,
                       const unsigned char **sip, size_t *sip_length);

#endif
