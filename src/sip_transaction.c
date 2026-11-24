#include "sip_transaction.h"

void sip_tx_start(struct sip_transaction *tx, uint64_t now_ms)
{
    tx->next_ms = now_ms;
    tx->deadline_ms = now_ms + 32000;
    tx->interval_ms = 500;
    tx->proceeding = 0;
    tx->done = 0;
}

enum sip_tx_action sip_tx_poll(struct sip_transaction *tx, uint64_t now_ms)
{
    unsigned int delay;
    if (tx->done) return SIP_TX_DONE;
    if (now_ms >= tx->deadline_ms) {
        tx->done = 1;
        return SIP_TX_TIMEOUT;
    }
    if (now_ms < tx->next_ms) return SIP_TX_WAIT;
    delay = tx->proceeding ? 4000 : tx->interval_ms;
    tx->next_ms = now_ms + delay;
    if (!tx->proceeding && tx->interval_ms < 4000) tx->interval_ms *= 2;
    return SIP_TX_SEND;
}

void sip_tx_response(struct sip_transaction *tx, unsigned int status, uint64_t now_ms)
{
    if (tx->done || status < 100 || status > 699) return;
    if (status >= 200) tx->done = 1;
    else {
        tx->proceeding = 1;
        tx->next_ms = now_ms + 4000;
    }
}
