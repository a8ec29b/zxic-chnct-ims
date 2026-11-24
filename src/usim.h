#ifndef USIM_H
#define USIM_H

#include "at_http.h"

struct usim_challenge;
struct usim {
    struct at_http *at;
    int channel;
    struct usim_challenge *challenges;
};

enum usim_aka_status { USIM_AKA_SUCCESS, USIM_AKA_SYNC_FAILURE, USIM_AKA_REJECTED };
struct usim_aka_result {
    enum usim_aka_status status;
    unsigned char res[16], ck[16], ik[16], auts[14];
    size_t res_length;
};

/* The AT session must already be logged in. */
int usim_open(struct usim *sim, struct at_http *at);

/* Returns the raw APDU response including SW1/SW2. Never retries a command. */
int usim_transmit(struct usim *sim, const unsigned char *apdu, size_t apdu_len,
                  unsigned char *response, size_t *response_len);

int usim_authenticate(struct usim *sim, const unsigned char rand[16],
                      const unsigned char autn[16], struct usim_aka_result *result);

int usim_close(struct usim *sim);

#endif
