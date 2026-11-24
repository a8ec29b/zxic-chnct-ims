#include "ims_transport.h"
#include "udp6.h"

#include <string.h>
#include <unistd.h>

#define LOG(s) ((void)write(STDERR_FILENO, s, sizeof(s) - 1))

static void setup(struct ims_transport_flow *flow, enum ims_integrity integrity,
                  const unsigned char *key, uint16_t local, uint16_t remote,
                  uint32_t incoming, uint32_t outgoing)
{
    flow->local_port = local; flow->remote_port = remote;
    flow->incoming.spi = incoming; flow->outgoing.spi = outgoing;
    flow->incoming.integrity = integrity; flow->outgoing.integrity = integrity;
    flow->incoming.key = key; flow->outgoing.key = key;
    flow->incoming.key_length = flow->outgoing.key_length = 16;
    flow->next_sequence = 1;
}

void ims_transport_init(struct ims_transport *t, const unsigned char local[16],
                        const unsigned char remote[16], const unsigned char ik[16],
                        const struct ims_security_parameters *client,
                        const struct ims_security_parameters *server)
{
    memset(t, 0, sizeof(*t));
    memcpy(t->local_address, local, 16); memcpy(t->remote_address, remote, 16);
    memcpy(t->integrity_key, ik, 16);
    setup(&t->flows[IMS_FLOW_CLIENT], server->integrity, t->integrity_key,
          client->port_client, server->port_server, client->spi_client, server->spi_server);
    setup(&t->flows[IMS_FLOW_SERVER], server->integrity, t->integrity_key,
          client->port_server, server->port_client, client->spi_server, server->spi_client);
}

int ims_transport_encode(struct ims_transport *t, enum ims_flow flow,
                         const void *sip, size_t sip_length,
                         unsigned char *packet, size_t *packet_length)
{
    struct ims_transport_flow *f;
    size_t udp_length;
    if (flow != IMS_FLOW_CLIENT && flow != IMS_FLOW_SERVER) return -1;
    f = &t->flows[flow];
    if (!f->next_sequence || *packet_length < 32 || sip_length > *packet_length - 32) return -1;
    /* Build UDP in place; ESP adds its header in front of it. */
    udp_length = *packet_length - 8;
    if (udp6_encode(t->local_address, t->remote_address, f->local_port, f->remote_port,
                    sip, sip_length, packet + 8, &udp_length) ||
        esp_null_encode(&f->outgoing, f->next_sequence, packet + 8, udp_length, 17,
                        packet, packet_length)) return -1;
    f->next_sequence++;
    return 0;
}

int ims_transport_decode(struct ims_transport *t, const unsigned char *packet, size_t length,
                         enum ims_flow *flow, const unsigned char **sip, size_t *sip_length)
{
    const unsigned char *udp;
    size_t udp_length;
    uint32_t spi;
    uint16_t source, destination;
    unsigned char next;
    struct ims_transport_flow *f;
    if (length < 4) return -1;
    spi = ((uint32_t)packet[0] << 24) | ((uint32_t)packet[1] << 16) |
          ((uint32_t)packet[2] << 8) | packet[3];
    if (spi == t->flows[IMS_FLOW_CLIENT].incoming.spi) *flow = IMS_FLOW_CLIENT;
    else if (spi == t->flows[IMS_FLOW_SERVER].incoming.spi) *flow = IMS_FLOW_SERVER;
    else { LOG("IMS ESP association not recognized.\n"); return -1; }
    f = &t->flows[*flow];
    if (esp_null_decode(&f->incoming, packet, length, &udp, &udp_length, &next)) {
        LOG("IMS ESP integrity or replay check failed.\n"); return -1;
    }
    if (next != 17) { LOG("IMS ESP payload protocol unsupported.\n"); return -1; }
    if (udp6_decode(t->remote_address, t->local_address, udp, udp_length,
                    &source, &destination, sip, sip_length)) {
        LOG("IMS UDP payload check failed.\n"); return -1;
    }
    if (source != f->remote_port || destination != f->local_port) {
        LOG("IMS UDP ports did not match.\n"); return -1;
    }
    return 0;
}
