#ifndef OPERATOR_PLMN_H
#define OPERATOR_PLMN_H

#include <string.h>

static inline int operator_supported_imsi(const char *imsi)
{
    static const char *const prefixes[] = {
        "46001", "46006", "46009",
        "46003", "46005", "46011", "46012"
    };
    size_t i;
    for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++)
        if (!strncmp(imsi, prefixes[i], 5)) return 1;
    return 0;
}

#endif
