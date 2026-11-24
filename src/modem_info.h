#ifndef MODEM_INFO_H
#define MODEM_INFO_H

#include <stddef.h>

struct ims_pdn_info {
    int cid;
    unsigned char address[16];
    char interface[16];
    unsigned char pcscf[2][16];
    size_t pcscf_count;
};

/* Parse AT+CIMI and require a mainland China Telecom home PLMN. */
int modem_read_imsi(const char *response, char imsi[16]);
int modem_telecom_imsi(const char *response, char imsi[16]);
int modem_context_active(const char *response, int cid);
int modem_context_is_ims(const char *response, int cid);
int modem_ims_pdn(const char *response, int cid, struct ims_pdn_info *info);

#endif
