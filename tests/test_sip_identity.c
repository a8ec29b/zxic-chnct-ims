#include "sip_identity.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char msg1[] = "SIP/2.0 200 OK\r\n"
        "Via: SIP/2.0/UDP [2001:db8::1]:5060;branch=z9hG4bKabc\r\n"
        "P-Associated-URI: <tel:+8613800000000>\r\n"
        "P-Associated-URI: <sip:+8613800000000@ims.example>\r\n"
        "Content-Length: 0\r\n\r\n";
    const char msg2[] = "SIP/2.0 200 OK\r\n"
        "Via: SIP/2.0/UDP [2001:db8::1]:5060;branch=z9hG4bKabc\r\n"
        "P-Associated-URI: <sip:a@ims.example>,<tel:+123>\r\n"
        "Content-Length: 0\r\n\r\n";
    const char msg3[] = "SIP/2.0 200 OK\r\n"
        "Via: SIP/2.0/UDP [2001:db8::1]:5060;branch=z9hG4bKabc\r\n"
        "P-Associated-URI: <tel:+123>, <tel:+456>\r\n"
        "Content-Length: 0\r\n\r\n";
    struct sip_message m;
    char out[512];
    assert(sip_parse(msg1, sizeof(msg1) - 1, &m) == 0);
    assert(sip_associated_uri(&m, out, sizeof(out)) == 1);
    assert(!strcmp(out, "sip:+8613800000000@ims.example"));
    assert(sip_parse(msg2, sizeof(msg2) - 1, &m) == 0);
    assert(sip_associated_uri(&m, out, sizeof(out)) == 1);
    assert(!strcmp(out, "sip:a@ims.example"));
    assert(sip_parse(msg3, sizeof(msg3) - 1, &m) == 0);
    assert(sip_associated_uri(&m, out, sizeof(out)) == 0);
    {
        const char short_uri[] = "SIP/2.0 200 OK\r\nP-Associated-URI: <s>\r\nContent-Length: 0\r\n\r\n";
        assert(sip_parse(short_uri, sizeof(short_uri) - 1, &m) == 0);
        assert(sip_associated_uri(&m, out, sizeof(out)) == 0);
    }
    puts("sip_identity: ok");
    return 0;
}
