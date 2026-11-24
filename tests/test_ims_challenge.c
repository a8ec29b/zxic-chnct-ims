#include "ims_challenge.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char input[] =
        "Digest realm=\"ims.example\",nonce=\"AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=\","
        "algorithm=AKAv1-MD5,qop=\"auth-int, auth\",opaque=\"value\"";
    static const char duplicate[] =
        "Digest realm=\"a\",realm=\"b\",nonce=\"AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=\",algorithm=AKAv1-MD5";
    struct sip_slice header = {input, sizeof(input) - 1};
    struct ims_challenge challenge;
    assert(ims_challenge_parse(&header, &challenge) == 0);
    assert(!strcmp(challenge.realm, "ims.example") && challenge.qop_auth);
    assert(challenge.rand[0] == 0 && challenge.rand[15] == 15);
    assert(challenge.autn[0] == 16 && challenge.autn[15] == 31);
    header.data = duplicate; header.length = sizeof(duplicate) - 1;
    assert(ims_challenge_parse(&header, &challenge) == -1);
    puts("ims_challenge: ok");
    return 0;
}
