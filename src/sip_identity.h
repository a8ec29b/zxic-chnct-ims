#ifndef SIP_IDENTITY_H
#define SIP_IDENTITY_H

#include "sip_message.h"
#include <stddef.h>

/* Extract the first sip: IMPU from P-Associated-URI headers. Returns 1 when
   one was found and copied, 0 otherwise. tel: URIs are skipped. */
int sip_associated_uri(const struct sip_message *message, char *out, size_t capacity);

#endif
