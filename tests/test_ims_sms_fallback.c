#include "../src/ims_runtime.c"
#include <assert.h>

static int sent, stored;
static enum ims_flow sent_flow;
static char seen_identity[256];

int ims_socket_send(int fd, struct ims_transport *transport, enum ims_flow flow,
                    const void *sip, size_t length)
{
    (void)fd; (void)transport; (void)sip; (void)length;
    sent++; sent_flow = flow;
    return 0;
}

int sms_incoming_process(sqlite3 *db, const void *packet, size_t packet_length,
                         sqlite3_int64 now, const char *response_tag,
                         const struct sms_report_request *config,
                         struct sms_incoming_result *result)
{
    struct sms_report_request request = *config;
    const unsigned char ack[] = {2, 7, 0x41, 2, 0, 0};
    (void)db; (void)packet; (void)packet_length; (void)now; (void)response_tag;
    stored++;
    snprintf(seen_identity, sizeof(seen_identity), "%s", config->identity);
    request.target = "sip:smsc@example.com";
    memset(result, 0, sizeof(*result));
    memcpy(result->sip_response, "SIP/2.0 200 OK", 14);
    result->sip_response_length = 14;
    return sms_report_build(&request, ack, sizeof(ack), result->report_message,
                            sizeof(result->report_message), &result->report_message_length);
}

static void response(struct runtime *r, int code, const char *id)
{
    char data[256]; struct sip_message parsed;
    int n = snprintf(data, sizeof(data), "SIP/2.0 %d Test\r\nCall-ID: %s\r\nCSeq: 1 MESSAGE\r\nContent-Length: 0\r\n\r\n", code, id);
    assert(n > 0 && sip_parse(data, (size_t)n, &parsed) == 0);
    assert(report_response(r, &parsed) == 1);
}

int main(void)
{
    static const char inbound[] =
        "MESSAGE sip:user@example.com SIP/2.0\r\nVia: SIP/2.0/UDP host;branch=z9hG4bKin\r\n"
        "From: <sip:smsc@example.com>;tag=r\r\nTo: <sip:user@example.com>\r\n"
        "Call-ID: incoming\r\nCSeq: 7 MESSAGE\r\nP-Asserted-Identity: <sip:smsc@example.com>\r\n"
        "Content-Length: 0\r\n\r\n";
    struct runtime r = {0}; struct sip_message parsed;
    struct report_slot *slot = &r.reports[0];
    char primary[64], fallback[64];
    strcpy(r.call_id, "test-call"); strcpy(r.tag, "test-tag"); strcpy(r.branch, "z9hG4bKtest");
    strcpy(r.identity.public_identity, "sip:old@example.com");
    strcpy(r.registration.associated_uri, "sip:new@example.com");
    strcpy(r.local_address, "2001:db8::1"); r.registration.request.security.port_server = 4001;
    assert(sip_parse(inbound, sizeof(inbound) - 1, &parsed) == 0);
    assert(receive_sms(&r, (const unsigned char *)inbound, sizeof(inbound) - 1, &parsed, IMS_FLOW_SERVER) == 0);
    assert(stored == 1 && sent_flow == IMS_FLOW_CLIENT && !strcmp(seen_identity, "sip:new@example.com"));
    assert(slot->fallback_length && strstr(slot->fallback_packet, "sip:old@example.com"));
    strcpy(primary, slot->call_id); strcpy(fallback, slot->fallback_call_id);
    assert(strcmp(primary, fallback));
    assert(receive_sms(&r, (const unsigned char *)inbound, sizeof(inbound) - 1, &parsed, IMS_FLOW_SERVER) == 0);
    assert(stored == 1 && sent_flow == IMS_FLOW_SERVER);
    response(&r, 403, primary);
    assert(slot->fallback_active && !strcmp(slot->call_id, fallback));
    assert(sent == 2);
    response(&r, 200, fallback);
    assert(r.sms_legacy && !r.queue_count);
    assert(receive_sms(&r, (const unsigned char *)inbound, sizeof(inbound) - 1, &parsed, IMS_FLOW_SERVER) == 0);
    assert(!strcmp(seen_identity, "sip:old@example.com") && sent_flow == IMS_FLOW_SERVER);
    puts("ims_sms_fallback: ok");
    return 0;
}
