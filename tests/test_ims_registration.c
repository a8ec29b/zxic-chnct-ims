#include "ims_registration.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int aka_calls;

static int authenticate(void *context, const unsigned char rand[16], const unsigned char autn[16],
                        struct usim_aka_result *result)
{
    (void)context;
    assert(rand[0] == 0 && rand[15] == 15 && autn[0] == 16 && autn[15] == 31);
    aka_calls++;
    memset(result, 0, sizeof(*result));
    result->status = USIM_AKA_SUCCESS;
    result->res_length = 8;
    memcpy(result->res, "12345678", 8);
    memset(result->ik, 1, 16);
    return 0;
}

int main(void)
{
    const char reply401[] = "SIP/2.0 401 Unauthorized\r\n"
        "Via: SIP/2.0/UDP [2001:db8::1]:5060;branch=z9hG4bKabc\r\n"
        "Call-ID: example\r\nCSeq: 1 REGISTER\r\n"
        "WWW-Authenticate: Digest realm=\"ims.example\",nonce=\"AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=\",algorithm=AKAv1-MD5,qop=\"auth\"\r\n"
        "Security-Server: ipsec-3gpp;alg=hmac-md5-96;ealg=null;mod=trans;prot=esp;spi-c=2001;spi-s=2002;port-c=5000;port-s=5001\r\n"
        "Content-Length: 0\r\n\r\n";
    const char reply200[] = "SIP/2.0 200 OK\r\n"
        "Via: SIP/2.0/UDP [2001:db8::1]:4001;branch=z9hG4bKabc-2\r\n"
        "Call-ID: example\r\nCSeq: 2 REGISTER\r\n"
        "Contact: <sip:user@[2001:db8::1]:4001>;expires=120\r\n"
        "Service-Route: <sip:route.example;lr>\r\nContent-Length: 0\r\n\r\n";
    struct ims_register_request request = {
        .private_identity="user@ims.example", .public_identity="sip:user@ims.example",
        .realm="ims.example", .register_uri="sip:ims.example", .contact_user="user",
        .local_address="2001:db8::1", .call_id="example", .from_tag="tag",
        .branch="z9hG4bKabc", .via_port=5060, .contact_port=5060, .expires=3600,
        .cseq=1, .security={IMS_HMAC_MD5_96,1001,1002,4000,4001},
        .cnonce="cafebabe", .nonce_count=1
    };
    const unsigned char local[16] = {0x20,1,0xdb,8,0,0,0,0,0,0,0,0,0,0,0,1};
    const unsigned char remote[16] = {0x20,1,0xdb,8,0,0,0,0,0,0,0,0,0,0,0,2};
    struct ims_registration reg;
    struct sip_message response;
    char packet[4096];
    size_t length;
    assert(ims_registration_init(&reg, &request, local, remote) == 0);
    assert(ims_registration_begin(&reg, packet, sizeof(packet), &length) == 0);
    assert(sip_parse(reply401, sizeof(reply401)-1, &response) == 0);
    assert(ims_registration_response(&reg, &response, 0, authenticate, NULL, packet, sizeof(packet), &length) == IMS_REG_SEND_PROTECTED);
    assert(aka_calls == 1 && reg.request.cseq == 2 && reg.transport.flows[IMS_FLOW_CLIENT].outgoing.spi == 2002);
    assert(strstr(packet, "Security-Verify:") && strstr(packet, "integrity-protected=yes"));
    assert(ims_registration_response(&reg, &response, 0, authenticate, NULL, packet, sizeof(packet), &length) == IMS_REG_IGNORE);
    assert(aka_calls == 1);
    assert(sip_parse(reply200, sizeof(reply200)-1, &response) == 0);
    assert(ims_registration_response(&reg, &response, 1, authenticate, NULL, packet, sizeof(packet), &length) == IMS_REG_REGISTERED);
    assert(!strcmp(reg.service_route, "<sip:route.example;lr>"));
    assert(reg.expires_seconds == 120);
    assert(ims_registration_refresh(&reg, packet, sizeof(packet), &length) == 0);
    assert(reg.request.cseq == 3 && reg.request.nonce_count == 2);
    assert(strstr(packet, "nc=00000002") && strstr(packet, "CSeq: 3 REGISTER"));
    puts("ims_registration: ok");
    return 0;
}
