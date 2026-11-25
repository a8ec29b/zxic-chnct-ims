#include "ims_identity.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned int selected;
static int restored, include_isim = 1, domain_missing, first_impu_empty;

int usim_transmit(struct usim *sim, const unsigned char *apdu, size_t apdu_len,
                  unsigned char *response, size_t *response_len)
{
    static const unsigned char transparent[] = {0x62,8,0x82,2,0x41,0x21,0x80,2,0,32};
    static const unsigned char records[] = {0x62,11,0x82,5,0x42,0x21,0,32,2,0x80,2,0,64};
    static const unsigned char isim[] = {0x61,9,0x4f,7,0xa0,0,0,0,0x87,0x10,4};
    const unsigned char *src = NULL;
    size_t n = 0;
    const char *value;
    (void)sim; (void)apdu_len;
    if (apdu[1] == 0xa4 && apdu[2] == 4) {
        restored = apdu[11] == 2;
    } else if (apdu[1] == 0xa4) {
        selected = ((unsigned int)apdu[5] << 8) | apdu[6];
        if (selected == 0x6f03 && domain_missing) {
            response[0] = 0x6a; response[1] = 0x82; *response_len = 2; return 0;
        }
        src = selected == 0x2f00 || selected == 0x6f04 ? records : transparent;
        n = selected == 0x2f00 || selected == 0x6f04 ? sizeof(records) : sizeof(transparent);
    } else if (apdu[1] == 0xb2 && selected == 0x2f00) {
        if (apdu[2] == 2 && include_isim) { src = isim; n = sizeof(isim); }
        else { memset(response, 0xff, 32); n = 32; }
    } else if (apdu[1] == 0xb2 && selected == 0x6f04 && apdu[2] == 1 && first_impu_empty) {
        memset(response, 0xff, 32); n = 32;
    } else if (apdu[1] == 0xb0 || apdu[1] == 0xb2) {
        value = selected == 0x6f02 ? "alice@ims.example.com" :
                selected == 0x6f03 ? "ims.example.com" : "sip:alice@ims.example.com";
        n = strlen(value); response[0] = 0x80; response[1] = (unsigned char)n;
        memcpy(response + 2, value, n); n += 2;
    } else return -1;
    if (src) memcpy(response, src, n);
    assert(*response_len >= n + 2);
    response[n++] = 0x90; response[n++] = 0;
    *response_len = n;
    return 0;
}

int main(void)
{
    struct usim sim = {0};
    struct ims_identity identity;
    assert(ims_identity_read(&sim, &identity) == 0 && restored);
    assert(!strcmp(identity.private_identity, "alice@ims.example.com"));
    assert(!strcmp(identity.public_identity, "sip:alice@ims.example.com"));
    assert(!strcmp(identity.domain, "ims.example.com") && identity.isim_aid_length == 7);
    domain_missing = 1; first_impu_empty = 1;
    assert(ims_identity_read(&sim, &identity) == 0 && restored);
    assert(!strcmp(identity.domain, "ims.example.com"));
    assert(!strcmp(identity.public_identity, "sip:alice@ims.example.com"));
    include_isim = 0;
    assert(ims_identity_read(&sim, &identity) == 1 && restored);
    assert(ims_identity_from_imsi("460110123456789", &identity) == 0);
    assert(!strcmp(identity.domain, "ims.mnc011.mcc460.3gppnetwork.org"));
    assert(ims_identity_from_imsi("460011234567890", &identity) == 0);
    assert(!strcmp(identity.domain, "ims.mnc001.mcc460.3gppnetwork.org"));
    assert(!strcmp(identity.private_identity, "460011234567890@ims.mnc001.mcc460.3gppnetwork.org"));
    assert(ims_identity_from_imsi("460061234567890", &identity) == 0);
    assert(!strcmp(identity.domain, "ims.mnc006.mcc460.3gppnetwork.org"));
    assert(ims_identity_from_imsi("460091234567890", &identity) == 0);
    assert(!strcmp(identity.domain, "ims.mnc009.mcc460.3gppnetwork.org"));
    assert(ims_identity_from_imsi("460001234567890", &identity) == -1);
    puts("ims_identity: ok");
    return 0;
}
