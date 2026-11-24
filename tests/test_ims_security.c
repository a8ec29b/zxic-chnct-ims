#include "ims_security.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char offered[] =
        "ipsec-3gpp;alg=hmac-sha-1-96;ealg=null;mod=trans;prot=esp;"
        "spi-c=20000;spi-s=20001;port-c=61000;port-s=61001;q=0.8,"
        "ipsec-3gpp;alg=hmac-md5-96;ealg=aes-cbc;mod=trans;prot=esp;"
        "spi-c=20000;spi-s=20001;port-c=61000;port-s=61001;q=1";
    struct ims_security_parameters local = {IMS_HMAC_MD5_96, 10000, 10001, 60000, 60001};
    struct ims_security_parameters selected;
    struct sip_slice header = {offered, sizeof(offered) - 1};
    char client[512];
    assert(ims_security_server(&header, &local, &selected) == 0);
    assert(selected.integrity == IMS_HMAC_SHA1_96 && selected.spi_client == 20000);
    assert(selected.spi_server == 20001 && selected.port_server == 61001);
    assert(ims_security_client(&local, client, sizeof(client)) == 0);
    assert(strstr(client, "ealg=null") != NULL && strstr(client, "aes") == NULL);
    puts("ims_security: ok");
    return 0;
}
