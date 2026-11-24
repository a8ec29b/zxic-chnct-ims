#include "sms_report.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

int sms_report_target(const struct sip_message *message, char *out, size_t capacity)
{
    const struct sip_slice *header = sip_header_get(message, "p-asserted-identity");
    const char *start, *end, *bracket;
    size_t n;
    if (!header) return -1;
    start = header->data; end = start + header->length;
    while (start < end && (*start == ' ' || *start == '\t')) start++;
    bracket = memchr(start, '<', (size_t)(end - start));
    if (bracket) {
        start = bracket + 1; end = memchr(start, '>', (size_t)(end - start));
        if (!end) return -1;
    } else {
        bracket = memchr(start, ',', (size_t)(end - start));
        if (bracket) end = bracket;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
    }
    n = (size_t)(end - start);
    if (n < 5 || n >= capacity || strncasecmp(start, "sip:", 4) ||
        memchr(start, '\r', n) || memchr(start, '\n', n)) return -1;
    memcpy(out, start, n); out[n] = 0;
    return 0;
}

static int safe(const char *s)
{
    return s && !strchr(s, '\r') && !strchr(s, '\n');
}

static int add_slice(char *out, size_t capacity, size_t *used,
                     const char *name, const struct sip_slice *value)
{
    int n;
    if (!value) return 0;
    if (value->length > 2048 || memchr(value->data, '\r', value->length) ||
        memchr(value->data, '\n', value->length)) return -1;
    n = snprintf(out + *used, capacity - *used, "%s: %.*s\r\n", name,
                 (int)value->length, value->data);
    if (n < 0 || (size_t)n >= capacity - *used) return -1;
    *used += (size_t)n;
    return 0;
}

int sms_report_build(const struct sms_report_request *r,
                     const unsigned char *rp, size_t rp_length,
                     char *out, size_t capacity, size_t *length)
{
    size_t used;
    int n;
    if (!safe(r->target) || !safe(r->identity) || !safe(r->local_address) ||
        !safe(r->call_id) || !safe(r->tag) || !safe(r->branch) || !r->local_port || !r->cseq) return -1;
    n = snprintf(out, capacity,
        "MESSAGE %s SIP/2.0\r\nVia: SIP/2.0/UDP [%s]:%u;branch=%s;rport\r\n"
        "Max-Forwards: 70\r\nFrom: <%s>;tag=%s\r\nTo: <%s>\r\n"
        "Call-ID: %s\r\nCSeq: %u MESSAGE\r\nP-Preferred-Identity: <%s>\r\n"
        "Request-Disposition: no-fork\r\nAccept-Contact: *;+g.3gpp.smsip;require;explicit\r\n",
        r->target, r->local_address, r->local_port, r->branch, r->identity, r->tag,
        r->target, r->call_id, r->cseq, r->identity);
    if (n < 0 || (size_t)n >= capacity) return -1;
    used = (size_t)n;
    if (add_slice(out, capacity, &used, "In-Reply-To", r->in_reply_to) ||
        add_slice(out, capacity, &used, "Security-Verify", r->security_verify) ||
        add_slice(out, capacity, &used, "Route", r->service_route)) return -1;
    n = snprintf(out + used, capacity - used,
        "Content-Type: application/vnd.3gpp.sms\r\nContent-Transfer-Encoding: binary\r\n"
        "Content-Length: %zu\r\n\r\n", rp_length);
    if (n < 0 || (size_t)n >= capacity - used) return -1;
    used += (size_t)n;
    if (rp_length > capacity - used) return -1;
    memcpy(out + used, rp, rp_length); *length = used + rp_length;
    return 0;
}
