#ifndef SMS_RECEIPT_H
#define SMS_RECEIPT_H

#include <sqlite3.h>
#include <stddef.h>

/* transaction_key identifies the incoming SIP transaction; now is Unix time.
 * report is an RPDU to send in a separate SIP MESSAGE. */
int sms_receipt(sqlite3 *db, const char *transaction_key, sqlite3_int64 now, const unsigned char *rp,
                size_t rp_length, unsigned char report[9], size_t *report_length,
                int *completed);

#endif
