#include "ims_transport.h"
#include "udp6.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const unsigned char local[16] = {0x20,1,0xdb,8,0,0,0,0,0,0,0,0,0,0,0,1};
    const unsigned char remote[16] = {0x20,1,0xdb,8,0,0,0,0,0,0,0,0,0,0,0,2};
    const unsigned char key[16] = {1,2,3,4};
    struct ims_security_parameters client = {IMS_HMAC_MD5_96, 1001,1002,4000,4001};
    struct ims_security_parameters server = {IMS_HMAC_MD5_96, 2001,2002,5000,5001};
    struct ims_transport t;
    unsigned char packet[512], udp[256];
    const unsigned char *sip;
    size_t length, udp_length, sip_length;
    enum ims_flow flow;
    int i;
    ims_transport_init(&t, local, remote, key, &client, &server);
    for (i = 0; i < 2; i++) {
        struct ims_transport_flow *f = &t.flows[i];
        struct esp_null_sa peer = f->incoming;
        length = sizeof(packet);
        assert(ims_transport_encode(&t, (enum ims_flow)i, "hello", 5, packet, &length) == 0);
        assert(packet[3] == (unsigned char)(i ? 2001 : 2002));
        assert(f->next_sequence == 2);
        udp_length = sizeof(udp);
        assert(udp6_encode(remote, local, f->remote_port, f->local_port,
                           "MESSAGE", 7, udp, &udp_length) == 0);
        length = sizeof(packet);
        assert(esp_null_encode(&peer, 1, udp, udp_length, 17, packet, &length) == 0);
        assert(ims_transport_decode(&t, packet, length, &flow, &sip, &sip_length) == 0);
        assert(flow == (enum ims_flow)i && sip_length == 7 && !memcmp(sip, "MESSAGE", 7));
        assert(ims_transport_decode(&t, packet, length, &flow, &sip, &sip_length) == -1);
    }
    puts("ims_transport: ok");
    return 0;
}
