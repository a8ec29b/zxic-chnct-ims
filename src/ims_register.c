#include "ims_register.h"
#include "mbedtls/base64.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

struct writer { char *data; size_t used, cap; };

static int print(struct writer *w, const char *format, ...)
{
    va_list args;
    int n;
    va_start(args, format);
    n = vsnprintf(w->data + w->used, w->cap - w->used, format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= w->cap - w->used) return -1;
    w->used += (size_t)n;
    return 0;
}

static int safe(const char *s)
{
    return s && !strchr(s, '\r') && !strchr(s, '\n');
}

static int quoted(struct writer *w, const char *s)
{
    if (!safe(s) || print(w, "\"")) return -1;
    while (*s) {
        if ((*s == '"' || *s == '\\') && print(w, "\\")) return -1;
        if (print(w, "%c", *s++)) return -1;
    }
    return print(w, "\"");
}

static int attribute(struct writer *w, const char *name, const char *value)
{
    return print(w, ",%s=", name) || quoted(w, value);
}

int ims_register_build(const struct ims_register_request *r,
                       char *output, size_t capacity, size_t *length)
{
    struct writer w = {output, 0, capacity};
    const struct ims_challenge *c = r->challenge;
    const struct usim_aka_result *aka = r->aka;
    const char *realm = c ? c->realm : r->realm;
    const char *nonce = c ? c->nonce : "";
    char response[33] = "", security[512], auts[32];
    size_t auts_length;
    int authenticated = c && aka;
    if (!capacity || !safe(r->private_identity) || !safe(r->public_identity) ||
        !safe(realm) || !safe(r->register_uri) || !safe(r->contact_user) ||
        !safe(r->local_address) || !safe(r->call_id) || !safe(r->from_tag) ||
        !safe(r->branch) || !r->cseq || !r->via_port || !r->contact_port ||
        (!!c != !!aka) || (authenticated && !r->security_verify) ||
        ims_security_client(&r->security, security, sizeof(security))) return -1;
    if (authenticated && aka->status == USIM_AKA_SUCCESS) {
        if ((c->qop_auth && (!safe(r->cnonce) || !r->nonce_count)) ||
            ims_aka_v1_digest(r->private_identity, realm, r->register_uri, nonce,
                              aka->res, aka->res_length, c->qop_auth ? r->cnonce : NULL,
                              r->nonce_count, response)) return -1;
    } else if (authenticated && aka->status != USIM_AKA_SYNC_FAILURE) return -1;

    if (print(&w, "REGISTER %s SIP/2.0\r\nVia: SIP/2.0/UDP [%s]:%u;branch=%s;rport\r\n"
                  "Max-Forwards: 70\r\nTo: <%s>\r\nFrom: <%s>;tag=%s\r\n"
                  "Call-ID: %s\r\nCSeq: %u REGISTER\r\n"
                  "Contact: <sip:%s@[%s]:%u>;+g.3gpp.smsip;+g.3gpp.smsip-msisdn-less\r\n"
                  "Expires: %u\r\nSupported: path, sec-agree\r\nRequire: sec-agree\r\nProxy-Require: sec-agree\r\n"
                  "Security-Client: %s\r\n",
              r->register_uri, r->local_address, r->via_port, r->branch,
              r->public_identity, r->public_identity, r->from_tag, r->call_id,
              (unsigned int)r->cseq, r->contact_user, r->local_address,
              r->contact_port, r->expires, security)) return -1;
    if (authenticated) {
        size_t i;
        for (i = 0; i < r->security_verify->length; i++)
            if (r->security_verify->data[i] == '\r' || r->security_verify->data[i] == '\n') return -1;
        if (r->security_verify->length > 2048 ||
            print(&w, "Security-Verify: %.*s\r\n", (int)r->security_verify->length,
                  r->security_verify->data)) return -1;
    }
    if (print(&w, "%s: Digest nonce=", r->proxy_authorization ? "Proxy-Authorization" : "Authorization") || quoted(&w, nonce) ||
        attribute(&w, "uri", r->register_uri) || attribute(&w, "realm", realm) ||
        attribute(&w, "username", r->private_identity) || attribute(&w, "response", response) ||
        print(&w, ",algorithm=AKAv1-MD5,integrity-protected=%s", authenticated ? "yes" : "no")) return -1;
    if (authenticated && aka->status == USIM_AKA_SUCCESS && c->qop_auth) {
        if (print(&w, ",qop=auth,nc=%08x", (unsigned int)r->nonce_count) ||
            attribute(&w, "cnonce", r->cnonce)) return -1;
    }
    if (authenticated && aka->status == USIM_AKA_SYNC_FAILURE) {
        if (mbedtls_base64_encode((unsigned char *)auts, sizeof(auts), &auts_length,
                                  aka->auts, sizeof(aka->auts))) return -1;
        auts[auts_length] = 0;
        if (attribute(&w, "auts", auts)) return -1;
    }
    if (authenticated && c->opaque[0] && attribute(&w, "opaque", c->opaque)) return -1;
    if (print(&w, "\r\nContent-Length: 0\r\n\r\n")) return -1;
    *length = w.used;
    return 0;
}
