#ifndef SIP_TRANSACTION_H
#define SIP_TRANSACTION_H

#include <stdint.h>

enum sip_tx_action { SIP_TX_WAIT, SIP_TX_SEND, SIP_TX_TIMEOUT, SIP_TX_DONE };

struct sip_transaction {
    uint64_t next_ms, deadline_ms;
    unsigned int interval_ms;
    int proceeding, done;
};

/* Times use the caller's monotonic clock; poll also returns the initial send. */
void sip_tx_start(struct sip_transaction *tx, uint64_t now_ms);
enum sip_tx_action sip_tx_poll(struct sip_transaction *tx, uint64_t now_ms);
void sip_tx_response(struct sip_transaction *tx, unsigned int status, uint64_t now_ms);

#endif
