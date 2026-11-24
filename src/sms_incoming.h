#ifndef SMS_INCOMING_H
#define SMS_INCOMING_H

#include "sms_report.h"
#include <sqlite3.h>

struct sms_incoming_result {
    char sip_response[2048], report_message[4096];
    size_t sip_response_length, report_message_length;
    int completed;
};

/* Build the SIP response and, after durable storage, the separate RP report. */
int sms_incoming_process(sqlite3 *db, const void *packet, size_t packet_length,
                         sqlite3_int64 now, const char *response_tag,
                         const struct sms_report_request *report_config,
                         struct sms_incoming_result *result);

#endif
