#include "ims_registration.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int matches(const struct sip_slice *value, const char *text)
{
    size_t n = strlen(text);
    return value && value->length == n && !memcmp(value->data, text, n);
}

static int response_matches(const struct ims_registration *r, const struct sip_message *message)
{
    const struct sip_slice *call_id = sip_header_get(message, "call-id");
    const struct sip_slice *cseq = sip_header_get(message, "cseq");
    const struct sip_slice *via = sip_header_get(message, "via");
    char expected[32];
    size_t i, n = strlen(r->request.branch);
    if (!matches(call_id, r->request.call_id) || !via) return 0;
    snprintf(expected, sizeof(expected), "%u REGISTER", (unsigned int)r->request.cseq);
    if (!matches(cseq, expected)) return 0;
    for (i = 0; i + 8 + n <= via->length; i++) {
        if (!memcmp(via->data + i, ";branch=", 8) &&
            !memcmp(via->data + i + 8, r->request.branch, n) &&
            (i + 8 + n == via->length || via->data[i + 8 + n] == ';' ||
             via->data[i + 8 + n] == ',' || via->data[i + 8 + n] == ' ')) return 1;
    }
    return 0;
}

static int next_branch(struct ims_registration *r)
{
    int n;
    if (r->request.cseq == 0x7fffffffU) return -1;
    r->request.cseq++;
    n = snprintf(r->branch, sizeof(r->branch), "%s-%u", r->branch_base,
                 (unsigned int)r->request.cseq);
    if (n < 0 || (size_t)n >= sizeof(r->branch)) return -1;
    r->request.branch = r->branch;
    return 0;
}

static int number(const char *p, const char *end, unsigned int *value)
{
    unsigned long n = 0;
    const char *start = p;
    while (p < end && *p >= '0' && *p <= '9') {
        unsigned int digit = (unsigned int)(*p++ - '0');
        if (n > (0x7fffffffUL - digit) / 10) return -1;
        n = n * 10 + digit;
    }
    if (p == start || (p < end && *p != ';' && *p != ',' && *p != ' ' && *p != '\t')) return -1;
    *value = (unsigned int)n;
    return 0;
}

static int response_expiry(const struct ims_registration *r, const struct sip_message *message,
                           unsigned int *value)
{
    const struct sip_slice *header;
    size_t i, n = strlen(r->request.contact_user);
    *value = r->request.expires;
    for (i = 0; i < message->header_count; i++) {
        const struct sip_header *h = &message->headers[i];
        const char *start, *end, *p, *contact = NULL, *item_end;
        if (h->name.length != 7 || strncasecmp(h->name.data, "contact", 7)) continue;
        start = h->value.data; end = start + h->value.length;
        for (p = start; p + n + 5 <= end; p++) {
            if (!strncasecmp(p, "<sip:", 5) && !memcmp(p + 5, r->request.contact_user, n) &&
                p + 5 + n < end && p[5 + n] == '@') { contact = p; break; }
        }
        if (!contact) continue;
        item_end = memchr(contact, ',', (size_t)(end - contact));
        if (!item_end) item_end = end;
        for (p = contact; p + 9 <= item_end; p++) {
            if (!strncasecmp(p, ";expires=", 9)) return number(p + 9, item_end, value);
        }
    }
    header = sip_header_get(message, "expires");
    return header ? number(header->data, header->data + header->length, value) : 0;
}

int ims_registration_init(struct ims_registration *r, const struct ims_register_request *request,
                          const unsigned char local[16], const unsigned char remote[16])
{
    size_t branch_length = strlen(request->branch);
    if (!branch_length || branch_length >= sizeof(r->branch_base)) return -1;
    memset(r, 0, sizeof(*r)); r->request = *request;
    memcpy(r->local_address, local, 16); memcpy(r->remote_address, remote, 16);
    memcpy(r->branch_base, request->branch, branch_length + 1);
    memcpy(r->branch, request->branch, branch_length + 1);
    r->request.branch = r->branch;
    r->request.challenge = NULL; r->request.aka = NULL; r->request.security_verify = NULL;
    r->request.proxy_authorization = 0;
    r->state = IMS_REG_INITIAL;
    return 0;
}

int ims_registration_begin(struct ims_registration *r, char *packet, size_t capacity, size_t *length)
{
    if (r->state != IMS_REG_INITIAL) return -1;
    return ims_register_build(&r->request, packet, capacity, length);
}

int ims_registration_refresh(struct ims_registration *r, char *packet, size_t capacity, size_t *length)
{
    if (r->state != IMS_REG_READY || r->request.nonce_count == 0xffffffffU || next_branch(r)) return -1;
    r->request.nonce_count++;
    r->state = IMS_REG_AUTHENTICATED;
    return ims_register_build(&r->request, packet, capacity, length);
}

enum ims_reg_event ims_registration_response(struct ims_registration *r,
    const struct sip_message *response, int protected_transport, ims_reg_aka_fn authenticate,
    void *context, char *packet, size_t capacity, size_t *length)
{
    const struct sip_slice *challenge, *security, *route;
    unsigned int status;
    const char *line = response->start_line.data;
    if (r->state == IMS_REG_READY || r->state == IMS_REG_FAILED ||
        response->start_line.length < 12 || memcmp(line, "SIP/2.0 ", 8) ||
        line[8] < '1' || line[8] > '6' || line[9] < '0' || line[9] > '9' ||
        line[10] < '0' || line[10] > '9' || !response_matches(r, response)) return IMS_REG_IGNORE;
    status = (unsigned int)(line[8] - '0') * 100 + (unsigned int)(line[9] - '0') * 10 + (unsigned int)(line[10] - '0');
    if (status < 200) return IMS_REG_IGNORE;
    r->failure = IMS_REG_FAILURE_RESPONSE;
    if (status == 200 && r->state == IMS_REG_AUTHENTICATED && protected_transport) {
        if (response_expiry(r, response, &r->expires_seconds) || !r->expires_seconds) goto error;
        route = sip_header_get(response, "service-route");
        if (route && route->length < sizeof(r->service_route)) {
            memcpy(r->service_route, route->data, route->length); r->service_route[route->length] = 0;
        }
        r->state = IMS_REG_READY;
        return IMS_REG_REGISTERED;
    }
    if ((status != 401 && status != 407) ||
        (r->state != IMS_REG_INITIAL && r->state != IMS_REG_RESYNC &&
         !(r->state == IMS_REG_AUTHENTICATED && protected_transport))) goto error;
    challenge = sip_header_get(response, status == 401 ? "www-authenticate" : "proxy-authenticate");
    security = sip_header_get(response, "security-server");
    r->failure = IMS_REG_FAILURE_CHALLENGE;
    if (!challenge || ims_challenge_parse(challenge, &r->challenge)) goto error;
    r->failure = IMS_REG_FAILURE_SECURITY;
    if (!security || security->length >= sizeof(r->security_verify) ||
        ims_security_server(security, &r->request.security, &r->server)) goto error;
    memcpy(r->security_verify, security->data, security->length);
    r->security_verify[security->length] = 0;
    r->verify.data = r->security_verify; r->verify.length = security->length;
    memset(&r->aka, 0, sizeof(r->aka));
    r->failure = IMS_REG_FAILURE_AKA;
    if (authenticate(context, r->challenge.rand, r->challenge.autn, &r->aka) ||
        r->aka.status == USIM_AKA_REJECTED || next_branch(r)) goto error;
    r->request.challenge = &r->challenge; r->request.aka = &r->aka;
    r->request.security_verify = &r->verify; r->request.proxy_authorization = status == 407;
    r->request.nonce_count = 1;
    if (r->aka.status == USIM_AKA_SYNC_FAILURE) {
        if (r->resync_attempted) goto error;
        r->resync_attempted = 1; r->state = IMS_REG_RESYNC;
        r->failure = IMS_REG_FAILURE_BUILD;
        if (ims_register_build(&r->request, packet, capacity, length)) goto error;
        return IMS_REG_SEND_CLEAR;
    }
    r->request.via_port = r->request.security.port_server;
    r->request.contact_port = r->request.security.port_server;
    ims_transport_init(&r->transport, r->local_address, r->remote_address, r->aka.ik,
                       &r->request.security, &r->server);
    r->state = IMS_REG_AUTHENTICATED;
    r->failure = IMS_REG_FAILURE_BUILD;
    if (ims_register_build(&r->request, packet, capacity, length)) goto error;
    return IMS_REG_SEND_PROTECTED;
error:
    r->state = IMS_REG_FAILED;
    return IMS_REG_ERROR;
}
