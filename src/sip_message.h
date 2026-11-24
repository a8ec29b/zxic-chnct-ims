#ifndef SIP_MESSAGE_H
#define SIP_MESSAGE_H

#include <stddef.h>

#define SIP_MAX_HEADERS 64

struct sip_slice { const char *data; size_t length; };
struct sip_header { struct sip_slice name, value; };
struct sip_message {
    struct sip_slice start_line, body;
    struct sip_header headers[SIP_MAX_HEADERS];
    size_t header_count;
};

/* The parsed slices refer to the caller-owned datagram. */
int sip_parse(const void *packet, size_t length, struct sip_message *message);
const struct sip_slice *sip_header_get(const struct sip_message *message, const char *name);
int sip_reply_ok(const struct sip_message *request, const char *to_tag,
                 char *output, size_t capacity, size_t *length);
/* Stable key for retransmissions of an incoming SIP MESSAGE transaction. */
int sip_message_transaction_key(const struct sip_message *request, char *output, size_t capacity);

#endif
