#include "sms_incoming.h"
#include "sms_receipt.h"

#include <string.h>
#include <strings.h>

int sms_incoming_process(sqlite3 *db, const void *packet, size_t packet_length,
                         sqlite3_int64 now, const char *response_tag,
                         const struct sms_report_request *report_config,
                         struct sms_incoming_result *result)
{
    struct sip_message message;
    struct sms_report_request report = *report_config;
    const struct sip_slice *type;
    char transaction[1024], target[512];
    unsigned char rp_report[9];
    size_t rp_length;
    static const char content_type[] = "application/vnd.3gpp.sms";
    memset(result, 0, sizeof(*result));
    if (sip_parse(packet, packet_length, &message) ||
        sip_message_transaction_key(&message, transaction, sizeof(transaction))) return -1;
    type = sip_header_get(&message, "content-type");
    if (!type || type->length < sizeof(content_type) - 1 ||
        strncasecmp(type->data, content_type, sizeof(content_type) - 1) ||
        (type->length > sizeof(content_type) - 1 && type->data[sizeof(content_type) - 1] != ';') ||
        sms_report_target(&message, target, sizeof(target)) ||
        sip_reply_ok(&message, response_tag, result->sip_response, sizeof(result->sip_response),
                     &result->sip_response_length)) return -1;
    if (sms_receipt(db, transaction, now, (const unsigned char *)message.body.data,
                    message.body.length, rp_report, &rp_length, &result->completed)) return -1;
    report.target = target;
    report.in_reply_to = sip_header_get(&message, "call-id");
    if (sms_report_build(&report, rp_report, rp_length, result->report_message,
                         sizeof(result->report_message), &result->report_message_length)) return -1;
    return 0;
}
