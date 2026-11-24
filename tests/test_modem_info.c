#include "modem_info.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char response[] =
        "\r\n+CGCONTRDP: 2,5,\"ims.mnc011.mcc460.3gppnetwork.org\","
        "\"32.1.13.184.0.0.0.0.0.0.0.0.0.0.0.1\",\"\",\"\",\"\","
        "\"32.1.13.184.0.0.0.0.0.0.0.0.0.0.0.2\",\"2001:db8::3\"\r\n\r\nOK\r\n";
    struct ims_pdn_info info;
    char imsi[16];
    assert(modem_telecom_imsi("\r\n460110123456789\r\n\r\nOK\r\n", imsi) == 0);
    assert(!strcmp(imsi, "460110123456789"));
    assert(modem_telecom_imsi("\r\n+CIMI: 460110123456789\r\n\r\nOK\r\n", imsi) == 0);
    assert(!strcmp(imsi, "460110123456789"));
    assert(modem_telecom_imsi("\r\n+CIMI: 460001234567890\r\n\r\nOK\r\n", imsi) == -1);
    assert(modem_telecom_imsi("\r\n460001234567890\r\n\r\nOK\r\n", imsi) == -1);
    assert(modem_context_active("\r\n+CGACT: 1,1\r\n+CGACT: 2,0\r\n\r\nOK\r\n", 1) == 1);
    assert(modem_context_active("\r\n+CGACT: 1,1\r\n+CGACT: 2,0\r\n\r\nOK\r\n", 2) == 0);
    assert(modem_ims_pdn(response, 2, &info) == 0);
    assert(info.cid == 2 && info.pcscf_count == 2);
    assert(info.address[0] == 0x20 && info.address[1] == 1 && info.address[15] == 1);
    assert(info.pcscf[0][15] == 2 && info.pcscf[1][15] == 3);
    puts("modem_info: ok");
    return 0;
}
