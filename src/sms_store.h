#ifndef SMS_STORE_H
#define SMS_STORE_H

#include <sqlite3.h>

struct sms_part {
    /* Stable across retransmissions; distinct for different messages. */
    const char *group_key;
    const char *sender;
    const char *utf16be_hex;
    int sequence;
    int total;
    int concat_ref;
    const char *year;
    const char *month;
    const char *day;
    const char *hour;
    const char *minute;
    const char *second;
    const char *timezone;
    sqlite3_int64 epoch;
};

/* Open or initialize the vendor database and create the IMS receipt tables. */
int sms_store_open(sqlite3 **db, const char *path);

/* Persist one segment. On success the caller may acknowledge the segment.
 * completed is set to 1 only when this call added a message to sms. */
int sms_store_part(sqlite3 *db, const struct sms_part *part, int *completed);

/* Atomically group and persist a received segment using its SIP transaction key.
 * now is the current Unix time; completed has the same meaning as above. */
int sms_store_receive(sqlite3 *db, const struct sms_part *part, int concat_16bit,
                      const char *transaction_key, sqlite3_int64 now, int *completed);

/* Convert a decoded UTF-8 segment to the vendor's UTF-16BE hex format. */
int sms_store_encode(const char *utf8, char **hex);

#endif
