#define _POSIX_C_SOURCE 200809L
#include "ims_security.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static char *trim(char *s)
{
    size_t n;
    while (*s == ' ' || *s == '\t') s++;
    n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = 0;
    return s;
}

static int decimal(const char *s, unsigned long max, unsigned long *result)
{
    unsigned long n = 0;
    if (!*s) return -1;
    while (*s) {
        unsigned int digit = (unsigned char)*s++ - '0';
        if (digit > 9 || n > (max - digit) / 10) return -1;
        n = n * 10 + digit;
    }
    *result = n;
    return 0;
}

static int quality(const char *s)
{
    int n, count = 0, fraction = 0;
    if (*s != '0' && *s != '1') return -1;
    n = *s++ - '0';
    if (*s && *s++ != '.') return -1;
    while (*s) {
        if (*s < '0' || *s > '9' || ++count > 3 || (n == 1 && *s != '0')) return -1;
        fraction = fraction * 10 + (*s++ - '0');
    }
    while (count++ < 3) fraction *= 10;
    return n * 1000 + fraction;
}

int ims_security_server(const struct sip_slice *header,
                        const struct ims_security_parameters *local,
                        struct ims_security_parameters *selected)
{
    char buffer[2048], *offer, *offer_state, *part, *part_state;
    int best = 0;
    if (header->length >= sizeof(buffer)) return -1;
    memcpy(buffer, header->data, header->length); buffer[header->length] = 0;
    for (offer = strtok_r(buffer, ",", &offer_state); offer; offer = strtok_r(NULL, ",", &offer_state)) {
        struct ims_security_parameters candidate;
        int have_alg = 0, fields = 0, compatible = 1, q = 1000;
        memset(&candidate, 0, sizeof(candidate));
        part = strtok_r(offer, ";", &part_state);
        if (!part || strcasecmp(trim(part), "ipsec-3gpp")) continue;
        while ((part = strtok_r(NULL, ";", &part_state)) != NULL) {
            char *key = trim(part), *value = strchr(key, '=');
            unsigned long number;
            if (!value) continue;
            *value++ = 0; key = trim(key); value = trim(value);
            if (!strcasecmp(key, "alg")) {
                if (!strcasecmp(value, "hmac-md5-96")) candidate.integrity = IMS_HMAC_MD5_96;
                else if (!strcasecmp(value, "hmac-sha-1-96") || !strcasecmp(value, "hmac-sha1-96"))
                    candidate.integrity = IMS_HMAC_SHA1_96;
                else compatible = 0;
                have_alg = 1;
            } else if (!strcasecmp(key, "ealg")) { if (strcasecmp(value, "null")) compatible = 0; }
            else if (!strcasecmp(key, "prot")) { if (strcasecmp(value, "esp")) compatible = 0; }
            else if (!strcasecmp(key, "mod")) { if (strcasecmp(value, "trans")) compatible = 0; }
            else if (!strcasecmp(key, "q")) { q = quality(value); if (q < 0) compatible = 0; }
            else if (!strcasecmp(key, "spi-c") || !strcasecmp(key, "spi-s")) {
                if (decimal(value, 0xffffffffUL, &number) || number < 256) { compatible = 0; continue; }
                if (!strcasecmp(key, "spi-c")) { candidate.spi_client = (uint32_t)number; fields |= 1; }
                else { candidate.spi_server = (uint32_t)number; fields |= 2; }
            } else if (!strcasecmp(key, "port-c") || !strcasecmp(key, "port-s")) {
                if (decimal(value, 65535, &number) || !number || number == 5060 || number == 5061) { compatible = 0; continue; }
                if (!strcasecmp(key, "port-c")) { candidate.port_client = (uint16_t)number; fields |= 4; }
                else { candidate.port_server = (uint16_t)number; fields |= 8; }
            }
        }
        if (!compatible || !have_alg || fields != 15 || q <= best ||
            candidate.spi_client == candidate.spi_server || candidate.port_client == candidate.port_server ||
            candidate.spi_client == local->spi_client || candidate.spi_client == local->spi_server ||
            candidate.spi_server == local->spi_client || candidate.spi_server == local->spi_server) continue;
        *selected = candidate; best = q;
    }
    return best ? 0 : -1;
}

int ims_security_client(const struct ims_security_parameters *local, char *out, size_t cap)
{
    int n = snprintf(out, cap,
        "ipsec-3gpp;alg=hmac-md5-96;ealg=null;mod=trans;port-c=%u;port-s=%u;prot=esp;spi-c=%u;spi-s=%u,"
        "ipsec-3gpp;alg=hmac-sha-1-96;ealg=null;mod=trans;port-c=%u;port-s=%u;prot=esp;spi-c=%u;spi-s=%u",
        local->port_client, local->port_server, local->spi_client, local->spi_server,
        local->port_client, local->port_server, local->spi_client, local->spi_server);
    return n < 0 || (size_t)n >= cap ? -1 : 0;
}
