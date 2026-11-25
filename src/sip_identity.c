#include "sip_identity.h"

#include <string.h>
#include <strings.h>

/* The network assigns the public identity (IMPU) in P-Associated-URI. Some
   networks provision an MSISDN-based IMPU that differs from the ISIM-derived
   identity; SIP requests such as the RP-ACK must use the assigned IMPU. */
int sip_associated_uri(const struct sip_message *message, char *out, size_t capacity)
{
    size_t i;
    if (!capacity) return 0;
    for (i = 0; i < message->header_count; i++) {
        const struct sip_header *h = &message->headers[i];
        const char *p, *end, *start;
        if (h->name.length != 16 || strncasecmp(h->name.data, "p-associated-uri", 16)) continue;
        p = h->value.data; end = p + h->value.length;
        while (p < end) {
            while (p < end && (*p == ' ' || *p == '\t' || *p == ',')) p++;
            if (p >= end || *p != '<') {
                while (p < end && *p != ',') p++;
                continue;
            }
            start = ++p;
            while (p < end && *p != '>') p++;
            if (p >= end || p == start || (size_t)(p - start) >= capacity) {
                while (p < end && *p != ',') p++;
                continue;
            }
            if (p - start >= 4 && !strncasecmp(start, "sip:", 4)) {
                memcpy(out, start, (size_t)(p - start));
                out[p - start] = 0;
                return 1;
            }
            while (p < end && *p != ',') p++;
        }
    }
    return 0;
}
