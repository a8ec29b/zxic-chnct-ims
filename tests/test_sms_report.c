#include "sms_report.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char input[] = "MESSAGE sip:user@example.com SIP/2.0\r\n"
        "P-Asserted-Identity: <sip:smsc@example.com>\r\nCall-ID: inbound\r\n"
        "Content-Length: 0\r\n\r\n";
    const unsigned char report[] = {2, 0x34, 0x41, 2, 0, 0};
    struct sip_message message, built;
    struct sms_report_request request = {
        .identity = "sip:user@example.com", .local_address = "2001:db8::1",
        .call_id = "outbound", .tag = "tag", .branch = "z9hG4bK123",
        .local_port = 4001, .cseq = 1
    };
    char target[128], output[2048];
    size_t length;
    assert(sip_parse(input, sizeof(input) - 1, &message) == 0);
    assert(sms_report_target(&message, target, sizeof(target)) == 0);
    assert(!strcmp(target, "sip:smsc@example.com"));
    request.target = target;
    request.in_reply_to = sip_header_get(&message, "call-id");
    assert(sms_report_build(&request, report, sizeof(report), output, sizeof(output), &length) == 0);
    assert(sip_parse(output, length, &built) == 0);
    assert(built.body.length == sizeof(report) && !memcmp(built.body.data, report, sizeof(report)));
    assert(sip_header_get(&built, "in-reply-to") != NULL);
    puts("sms_report: ok");
    return 0;
}
