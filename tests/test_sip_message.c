#include "sip_message.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char request[] =
        "MESSAGE sip:user@example.com SIP/2.0\r\n"
        "v: SIP/2.0/UDP [2001:db8::1]:5060;branch=z9hG4bKabc\r\n"
        "Via: SIP/2.0/UDP [2001:db8::2]:5060;branch=z9hG4bKxyz\r\n"
        "f: <sip:sender@example.com>;tag=remote\r\n"
        "t: <sip:user@example.com>\r\n"
        "i: call123\r\n"
        "CSeq: 42 MESSAGE\r\n"
        "c: application/vnd.3gpp.sms\r\n"
        "l: 4\r\n\r\n\x01\x00\x03\xff";
    static const char expected[] =
        "SIP/2.0 200 OK\r\n"
        "Via: SIP/2.0/UDP [2001:db8::1]:5060;branch=z9hG4bKabc\r\n"
        "Via: SIP/2.0/UDP [2001:db8::2]:5060;branch=z9hG4bKxyz\r\n"
        "From: <sip:sender@example.com>;tag=remote\r\n"
        "To: <sip:user@example.com>;tag=local\r\n"
        "Call-ID: call123\r\nCSeq: 42 MESSAGE\r\nContent-Length: 0\r\n\r\n";
    struct sip_message msg;
    char response[1024]; size_t length;
    assert(sip_parse(request, sizeof(request) - 1, &msg) == 0);
    assert(msg.body.length == 4 && (unsigned char)msg.body.data[1] == 0);
    assert(sip_reply_ok(&msg, "local", response, sizeof(response), &length) == 0);
    assert(length == sizeof(expected) - 1 && !memcmp(response, expected, length));
    assert(sip_parse(request, sizeof(request) - 2, &msg) == -1);
    assert(sip_parse("MESSAGE x SIP/2.0\r\nVia: x\r\n", 27, &msg) == -1);
    puts("sip_message: ok");
    return 0;
}
