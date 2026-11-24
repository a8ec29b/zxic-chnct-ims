#include "sip_message.h"

#include <stdio.h>
#include <string.h>

static int same(const struct sip_slice *value, const char *text)
{
    size_t i, len = strlen(text);
    if (value->length != len) return 0;
    for (i = 0; i < len; i++) {
        unsigned char a = (unsigned char)value->data[i], b = (unsigned char)text[i];
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if (a != b) return 0;
    }
    return 1;
}

static int name_is(const struct sip_slice *name, const char *wanted)
{
    if (same(name, wanted)) return 1;
    if (same(name, "v")) return !strcmp(wanted, "via");
    if (same(name, "f")) return !strcmp(wanted, "from");
    if (same(name, "t")) return !strcmp(wanted, "to");
    if (same(name, "i")) return !strcmp(wanted, "call-id");
    if (same(name, "l")) return !strcmp(wanted, "content-length");
    if (same(name, "c")) return !strcmp(wanted, "content-type");
    return 0;
}

const struct sip_slice *sip_header_get(const struct sip_message *message, const char *name)
{
    size_t i;
    for (i = 0; i < message->header_count; i++)
        if (name_is(&message->headers[i].name, name)) return &message->headers[i].value;
    return NULL;
}

static const char *line_end(const char *p, const char *end)
{
    while (p + 1 < end) {
        if (p[0] == '\r' && p[1] == '\n') return p;
        p++;
    }
    return NULL;
}

int sip_parse(const void *packet, size_t length, struct sip_message *message)
{
    const char *p = packet, *end = p + length, *line, *colon;
    const struct sip_slice *content_length;
    size_t n = 0, body_length = 0;
    int ended = 0;
    memset(message, 0, sizeof(*message));
    line = line_end(p, end);
    if (!line || line == p) return -1;
    message->start_line.data = p; message->start_line.length = (size_t)(line - p);
    p = line + 2;
    while (p < end) {
        line = line_end(p, end);
        if (!line) return -1;
        if (line == p) { p += 2; ended = 1; break; }
        if (*p == ' ' || *p == '\t' || n == SIP_MAX_HEADERS) return -1;
        colon = memchr(p, ':', (size_t)(line - p));
        if (!colon || colon == p) return -1;
        message->headers[n].name.data = p;
        message->headers[n].name.length = (size_t)(colon - p);
        p = colon + 1;
        while (p < line && (*p == ' ' || *p == '\t')) p++;
        while (line > p && (line[-1] == ' ' || line[-1] == '\t')) line--;
        message->headers[n].value.data = p;
        message->headers[n].value.length = (size_t)(line - p);
        n++;
        /* Find the original CRLF after trimming header whitespace. */
        line = line_end(line, end);
        if (!line) return -1;
        p = line + 2;
    }
    if (!ended || p > end) return -1;
    message->header_count = n;
    content_length = sip_header_get(message, "content-length");
    if (content_length) {
        size_t i;
        if (!content_length->length) return -1;
        for (i = 0; i < content_length->length; i++) {
            unsigned char c = (unsigned char)content_length->data[i];
            if (c < '0' || c > '9' || body_length > length / 10) return -1;
            body_length = body_length * 10 + c - '0';
        }
        if (body_length != (size_t)(end - p)) return -1;
    } else body_length = (size_t)(end - p);
    message->body.data = p; message->body.length = body_length;
    return 0;
}

static int append(char *out, size_t cap, size_t *used, const char *s, size_t n)
{
    if (n > cap - *used) return -1;
    memcpy(out + *used, s, n); *used += n;
    return 0;
}

static int add_header(char *out, size_t cap, size_t *used, const char *name,
                      const struct sip_slice *value)
{
    return append(out, cap, used, name, strlen(name)) || append(out, cap, used, ": ", 2) ||
        append(out, cap, used, value->data, value->length) || append(out, cap, used, "\r\n", 2);
}

static int has_tag(const struct sip_slice *to)
{
    size_t i;
    for (i = 0; i + 5 <= to->length; i++) {
        struct sip_slice candidate = {to->data + i, 5};
        if (same(&candidate, ";tag=")) return 1;
    }
    return 0;
}

int sip_reply_ok(const struct sip_message *request, const char *to_tag,
                 char *output, size_t capacity, size_t *length)
{
    const struct sip_slice *from, *to, *call_id, *cseq;
    size_t used = 0, i, vias = 0;
    from = sip_header_get(request, "from"); to = sip_header_get(request, "to");
    call_id = sip_header_get(request, "call-id"); cseq = sip_header_get(request, "cseq");
    if (!from || !to || !call_id || !cseq) return -1;
    if (append(output, capacity, &used, "SIP/2.0 200 OK\r\n", 16)) return -1;
    for (i = 0; i < request->header_count; i++) {
        if (name_is(&request->headers[i].name, "via")) {
            if (add_header(output, capacity, &used, "Via", &request->headers[i].value)) return -1;
            vias++;
        }
    }
    if (!vias || add_header(output, capacity, &used, "From", from) ||
        append(output, capacity, &used, "To: ", 4) ||
        append(output, capacity, &used, to->data, to->length)) return -1;
    if (!has_tag(to)) {
        if (!to_tag || !*to_tag || append(output, capacity, &used, ";tag=", 5) ||
            append(output, capacity, &used, to_tag, strlen(to_tag))) return -1;
    }
    if (append(output, capacity, &used, "\r\n", 2) ||
        add_header(output, capacity, &used, "Call-ID", call_id) ||
        add_header(output, capacity, &used, "CSeq", cseq) ||
        append(output, capacity, &used, "Content-Length: 0\r\n\r\n", 21)) return -1;
    *length = used;
    return 0;
}

int sip_message_transaction_key(const struct sip_message *request, char *output, size_t capacity)
{
    const struct sip_slice *call_id = sip_header_get(request, "call-id");
    const struct sip_slice *cseq = sip_header_get(request, "cseq");
    const struct sip_slice *via = sip_header_get(request, "via");
    const char *p, *end, *number, *number_end, *branch = NULL, *branch_end = NULL;
    int n;
    if (!call_id || !cseq || !via || request->start_line.length < 8 ||
        memcmp(request->start_line.data, "MESSAGE ", 8)) return -1;
    p = cseq->data; end = p + cseq->length;
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    number = p;
    while (p < end && *p >= '0' && *p <= '9') p++;
    number_end = p;
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    if (number == number_end || end - p != 7 || memcmp(p, "MESSAGE", 7)) return -1;
    p = via->data; end = p + via->length;
    while (p < end && *p != ',') {
        if (*p++ == ';') {
            const char *key = p;
            struct sip_slice name;
            while (p < end && *p != '=' && *p != ';' && *p != ',') p++;
            name.data = key; name.length = (size_t)(p - key);
            if (p < end && *p == '=' && same(&name, "branch")) {
                branch = ++p;
                while (p < end && *p != ';' && *p != ',' && *p != ' ' && *p != '\t') p++;
                branch_end = p; break;
            }
        }
    }
    if (!branch || branch == branch_end || call_id->length > 512 ||
        number_end - number > 20 || branch_end - branch > 255) return -1;
    n = snprintf(output, capacity, "%zu:%.*s:%.*s:%.*s", call_id->length,
                 (int)call_id->length, call_id->data, (int)(number_end - number), number,
                 (int)(branch_end - branch), branch);
    return n < 0 || (size_t)n >= capacity ? -1 : 0;
}
