#ifndef IMS_REGISTER_H
#define IMS_REGISTER_H

#include "ims_challenge.h"
#include "ims_security.h"
#include "usim.h"
#include <stdint.h>

struct ims_register_request {
    const char *private_identity, *public_identity, *realm, *register_uri;
    const char *contact_user, *local_address;
    const char *call_id, *from_tag, *branch;
    unsigned int via_port, contact_port, expires;
    uint32_t cseq;
    struct ims_security_parameters security;
    /* Both are NULL for the initial request. */
    const struct ims_challenge *challenge;
    const struct usim_aka_result *aka;
    int proxy_authorization;
    const char *cnonce;
    uint32_t nonce_count;
    /* Exact Security-Server value, for an authenticated request. */
    const struct sip_slice *security_verify;
};

/* Produces one UDP REGISTER datagram; the caller manages transactions and SAs. */
int ims_register_build(const struct ims_register_request *request,
                       char *output, size_t capacity, size_t *length);

#endif
