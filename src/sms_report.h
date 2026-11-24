#ifndef SMS_REPORT_H
#define SMS_REPORT_H

#include "sip_message.h"
#include <stddef.h>

struct sms_report_request {
    const char *target, *identity, *local_address;
    const char *call_id, *tag, *branch;
    const struct sip_slice *in_reply_to, *security_verify, *service_route;
    unsigned int local_port, cseq;
};

/* Extract the SMS service-centre SIP URI from P-Asserted-Identity. */
int sms_report_target(const struct sip_message *message, char *out, size_t capacity);
int sms_report_build(const struct sms_report_request *request,
                     const unsigned char *rp, size_t rp_length,
                     char *out, size_t capacity, size_t *length);

#endif
