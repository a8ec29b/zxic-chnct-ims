#ifndef IMS_APP_H
#define IMS_APP_H

#include "at_http.h"

enum ims_app_result {
    IMS_APP_OK = 0,
    IMS_APP_LOGIN_FAILED = 1,
    IMS_APP_SIM_UNAVAILABLE = 2,
    IMS_APP_UNSUPPORTED_OPERATOR = 3
};

typedef int (*ims_app_start_fn)(struct at_http *at, const char *imsi, void *context);

/* The callback is the only entry to device DB, USIM and IMS operations. */
/* Clears the supplied password as soon as login completes. */
int ims_app_run(struct at_http *at, char *password, ims_app_start_fn start, void *context);

#endif
