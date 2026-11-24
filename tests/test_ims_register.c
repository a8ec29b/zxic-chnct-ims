#include "ims_register.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    struct ims_register_request request = {
        .private_identity = "460110123456789@ims.mnc011.mcc460.3gppnetwork.org",
        .public_identity = "sip:460110123456789@ims.mnc011.mcc460.3gppnetwork.org",
        .realm = "ims.mnc011.mcc460.3gppnetwork.org",
        .register_uri = "sip:ims.mnc011.mcc460.3gppnetwork.org",
        .contact_user = "device", .local_address = "2001:db8::1",
        .call_id = "call", .from_tag = "tag", .branch = "z9hG4bKtest",
        .via_port = 5060, .contact_port = 5060, .expires = 3600, .cseq = 1,
        .security = {IMS_HMAC_MD5_96, 1001, 1002, 4000, 4001}
    };
    struct ims_challenge challenge = {
        .realm = "ims.mnc011.mcc460.3gppnetwork.org",
        .nonce = "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=",
        .opaque = "a\"b", .qop_auth = 1
    };
    struct usim_aka_result aka = {
        .status = USIM_AKA_SUCCESS, .res = {0,0x11,0x22,0xff,0x33,0x44,0x55,0x66}, .res_length = 8
    };
    const char verify[] = "ipsec-3gpp;alg=hmac-md5-96;ealg=null;spi-c=3000;spi-s=3001;port-c=4002;port-s=4003";
    struct sip_slice security = {verify, sizeof(verify) - 1};
    struct sip_message parsed;
    char output[4096]; size_t length;
    assert(ims_register_build(&request, output, sizeof(output), &length) == 0);
    assert(sip_parse(output, length, &parsed) == 0);
    assert(strstr(output, "response=\"\",algorithm=AKAv1-MD5,integrity-protected=no"));
    assert(strstr(output, "+g.3gpp.smsip"));
    assert(strstr(output, "Proxy-Require: sec-agree\r\n"));
    request.challenge = &challenge; request.aka = &aka; request.security_verify = &security;
    request.cnonce = "cafebabe"; request.nonce_count = 1; request.cseq = 2;
    request.via_port = 4001; request.contact_port = 4001;
    assert(ims_register_build(&request, output, sizeof(output), &length) == 0);
    assert(sip_parse(output, length, &parsed) == 0);
    assert(strstr(output, "response=\"fc3b00abb7fd9055c7287f3ff3bc9c64\""));
    assert(strstr(output, "qop=auth,nc=00000001,cnonce=\"cafebabe\""));
    assert(strstr(output, "opaque=\"a\\\"b\""));
    assert(strstr(output, "Security-Verify: ipsec-3gpp;"));
    assert(ims_register_build(&request, output, 20, &length) == -1);
    aka.status = USIM_AKA_SYNC_FAILURE;
    memset(aka.auts, 0, sizeof(aka.auts));
    assert(ims_register_build(&request, output, sizeof(output), &length) == 0);
    assert(strstr(output, "auts=\"AAAAAAAAAAAAAAAAAAA=\""));
    puts("ims_register: ok");
    return 0;
}
