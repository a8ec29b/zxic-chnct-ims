#ifndef IMS_PDN_H
#define IMS_PDN_H

#include "at_http.h"
#include "modem_info.h"
#include <stdio.h>

/* Find an active IMS PDN and its network-provided parameters. */
int ims_pdn_acquire(struct at_http *at, struct ims_pdn_info *info);
void ims_pdn_cleanup(void);
int ims_pdn_local_interface(struct ims_pdn_info *info, FILE *interfaces);
int ims_pdn_prepare_interface(struct ims_pdn_info *info);

#endif
