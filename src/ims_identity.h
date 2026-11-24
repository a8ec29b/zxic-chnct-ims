#ifndef IMS_IDENTITY_H
#define IMS_IDENTITY_H

#include "usim.h"

struct ims_identity {
    char private_identity[256], public_identity[256], domain[256];
    unsigned char isim_aid[16];
    size_t isim_aid_length;
};

/* Returns 0 for ISIM identity, 1 if ISIM is absent, -1 on a read error. */
int ims_identity_read(struct usim *sim, struct ims_identity *identity);

/* Construct the 3GPP IMS identities for an accepted China Telecom IMSI. */
int ims_identity_from_imsi(const char *imsi, struct ims_identity *identity);

#endif
