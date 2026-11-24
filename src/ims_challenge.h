#ifndef IMS_CHALLENGE_H
#define IMS_CHALLENGE_H

#include "sip_message.h"
#include <stddef.h>

struct ims_challenge {
    char realm[256], nonce[512], opaque[256];
    int qop_auth;
    unsigned char rand[16], autn[16];
};

/* Only the AKAv1-MD5 algorithm used by the current IMS profile is accepted. */
int ims_challenge_parse(const struct sip_slice *header, struct ims_challenge *challenge);

#endif
