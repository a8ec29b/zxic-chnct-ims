#include "sms_receipt.h"
#include "sms_pdu.h"
#include "sms_store.h"

#include <stdio.h>

int sms_receipt(sqlite3 *db, const char *transaction_key, sqlite3_int64 now, const unsigned char *rp,
                size_t rp_length, unsigned char report[9], size_t *report_length,
                int *completed)
{
    struct sms_deliver sms;
    struct sms_part part;
    char year[5], month[4], day[4], hour[4], minute[4], second[4], timezone[6];
    int rc;
    *completed = 0; *report_length = 0;
    if (rp_length < 2) return -1;
    if (sms_decode_rp_data(rp, rp_length, &sms)) {
        report[0] = 4; report[1] = rp[1]; report[2] = 1; report[3] = 96;
        *report_length = 4;
        return 0;
    }
    snprintf(year, sizeof(year), "%02d", sms.year % 100);
    snprintf(month, sizeof(month), "%02d", sms.month);
    snprintf(day, sizeof(day), "%02d", sms.day);
    snprintf(hour, sizeof(hour), "%02d", sms.hour);
    snprintf(minute, sizeof(minute), "%02d", sms.minute);
    snprintf(second, sizeof(second), "%02d", sms.second);
    snprintf(timezone, sizeof(timezone), "%+d", sms.timezone_quarters);
    part.group_key = NULL; part.sender = sms.sender; part.utf16be_hex = sms.utf16be_hex;
    part.sequence = (int)sms.sequence; part.total = (int)sms.total;
    part.concat_ref = (int)sms.concat_reference;
    part.year = year; part.month = month; part.day = day;
    part.hour = hour; part.minute = minute; part.second = second; part.timezone = timezone;
    part.epoch = sms.epoch;
    rc = sms_store_receive(db, &part, sms.concat_16bit, transaction_key, now, completed);
    if (rc == SQLITE_OK) {
        report[0] = 2; report[1] = sms.rp_reference; report[2] = 0x41;
        report[3] = 2; report[4] = 0; report[5] = 0;
        *report_length = 6;
    } else {
        report[0] = 4; report[1] = sms.rp_reference; report[2] = 1;
        report[3] = rc == SQLITE_FULL || rc == SQLITE_NOMEM ? 22 : 111;
        report[4] = 0x41; report[5] = 3; report[6] = 0;
        report[7] = report[3] == 22 ? 0xd3 : 0xff; report[8] = 0;
        *report_length = 9;
    }
    return 0;
}
