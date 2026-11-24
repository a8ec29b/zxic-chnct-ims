#ifndef IMS_RUNTIME_H
#define IMS_RUNTIME_H

#include "at_http.h"

/* Called only after ims_app_run has accepted the SIM operator. */
int ims_runtime_start(struct at_http *at, const char *imsi, void *context);

#endif
