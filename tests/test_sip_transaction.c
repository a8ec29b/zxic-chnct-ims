#include "sip_transaction.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    struct sip_transaction tx;
    sip_tx_start(&tx, 1000);
    assert(sip_tx_poll(&tx, 1000) == SIP_TX_SEND);
    assert(sip_tx_poll(&tx, 1499) == SIP_TX_WAIT);
    assert(sip_tx_poll(&tx, 1500) == SIP_TX_SEND);
    assert(sip_tx_poll(&tx, 2500) == SIP_TX_SEND);
    sip_tx_response(&tx, 100, 2700);
    assert(sip_tx_poll(&tx, 6699) == SIP_TX_WAIT);
    assert(sip_tx_poll(&tx, 6700) == SIP_TX_SEND);
    sip_tx_response(&tx, 401, 6800);
    assert(sip_tx_poll(&tx, 6800) == SIP_TX_DONE);
    sip_tx_start(&tx, 0);
    assert(sip_tx_poll(&tx, 32000) == SIP_TX_TIMEOUT);
    puts("sip_transaction: ok");
    return 0;
}
