#ifndef SMS_PDU_H
#define SMS_PDU_H

#include <stddef.h>

struct sms_deliver {
    unsigned char rp_reference;
    char sender[129];
    char utf16be_hex[1281];
    unsigned int concat_reference, sequence, total;
    int concat_16bit;
    int year, month, day, hour, minute, second, timezone_quarters;
    long long epoch;
};

/* Decode a network-to-MS RP-DATA containing one textual SMS-DELIVER. */
int sms_decode_rp_data(const unsigned char *data, size_t length, struct sms_deliver *out);

#endif
