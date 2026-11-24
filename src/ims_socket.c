#define _GNU_SOURCE
#include "ims_socket.h"

#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int ims_socket_open(const struct ims_transport *transport, const char *interface)
{
    struct sockaddr_in6 address;
    int fd = socket(AF_INET6, SOCK_RAW, IPPROTO_ESP);
    if (fd < 0) return -1;
    memset(&address, 0, sizeof(address));
    address.sin6_family = AF_INET6;
    memcpy(&address.sin6_addr, transport->local_address, 16);
    if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, interface, strlen(interface) + 1) ||
        bind(fd, (struct sockaddr *)&address, sizeof(address))) {
        close(fd);
        return -1;
    }
    return fd;
}

int ims_socket_send(int fd, struct ims_transport *transport, enum ims_flow flow,
                    const void *sip, size_t sip_length)
{
    unsigned char packet[4096];
    size_t length = sizeof(packet);
    struct sockaddr_in6 peer;
    ssize_t written;
    if (ims_transport_encode(transport, flow, sip, sip_length, packet, &length)) return -1;
    memset(&peer, 0, sizeof(peer));
    peer.sin6_family = AF_INET6;
    memcpy(&peer.sin6_addr, transport->remote_address, 16);
    written = sendto(fd, packet, length, 0, (struct sockaddr *)&peer, sizeof(peer));
    return written == (ssize_t)length ? 0 : -1;
}

int ims_socket_receive(int fd, struct ims_transport *transport,
                       unsigned char *packet, size_t capacity, enum ims_flow *flow,
                       const unsigned char **sip, size_t *sip_length)
{
    struct sockaddr_in6 peer;
    socklen_t peer_length = sizeof(peer);
    ssize_t length = recvfrom(fd, packet, capacity, 0, (struct sockaddr *)&peer, &peer_length);
    if (length < 0 || peer.sin6_family != AF_INET6 ||
        memcmp(&peer.sin6_addr, transport->remote_address, 16)) {
        (void)write(STDERR_FILENO, "IMS ESP sender did not match.\n", 30); return -1;
    }
    return ims_transport_decode(transport, packet, (size_t)length, flow, sip, sip_length);
}
