#include "modem_info.h"

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

int modem_telecom_imsi(const char *response, char imsi[16])
{
    const char *p = response, *end;
    size_t len;
    while (*p) {
        while (*p == '\r' || *p == '\n' || *p == ' ') p++;
        end = p; while (*end && *end != '\r' && *end != '\n') end++;
        if ((size_t)(end - p) >= 6 && !memcmp(p, "+CIMI:", 6)) {
            p += 6;
            while (p < end && (*p == ' ' || *p == '\t')) p++;
        }
        len = (size_t)(end - p);
        if (len >= 6 && len <= 15 && strspn(p, "0123456789") == len) {
            if (memcmp(p, "46003", 5) && memcmp(p, "46005", 5) &&
                memcmp(p, "46011", 5) && memcmp(p, "46012", 5)) return -1;
            memcpy(imsi, p, len); imsi[len] = 0;
            return 0;
        }
        p = end;
    }
    return -1;
}

int modem_context_active(const char *response, int cid)
{
    const char *p = response;
    char *end;
    long id, state;
    while ((p = strstr(p, "+CGACT:")) != NULL) {
        p += 7; id = strtol(p, &end, 10);
        if (end == p || *end != ',') return -1;
        p = end + 1; state = strtol(p, &end, 10);
        if (end == p || (state != 0 && state != 1)) return -1;
        if (id == cid) return (int)state;
        p = end;
    }
    return -1;
}

static int csv(const char *p, char fields[][256], size_t limit)
{
    size_t count = 0, n;
    while (*p && *p != '\r' && *p != '\n') {
        int quoted = 0;
        if (count == limit) return -1;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '"') { quoted = 1; p++; }
        n = 0;
        while (*p && *p != '\r' && *p != '\n' && (quoted ? *p != '"' : *p != ',')) {
            if (n + 1 >= 256) return -1;
            fields[count][n++] = *p++;
        }
        while (!quoted && n && (fields[count][n - 1] == ' ' || fields[count][n - 1] == '\t')) n--;
        fields[count++][n] = 0;
        if (quoted) {
            if (*p++ != '"') return -1;
            while (*p == ' ' || *p == '\t') p++;
        }
        if (*p != ',') break;
        p++;
    }
    return (int)count;
}

static int address6(const char *text, unsigned char address[16], int allow_mask)
{
    const char *p = text;
    char *end;
    int i, total = 0;
    if (strchr(text, ':')) return inet_pton(AF_INET6, text, address) == 1 ? 0 : -1;
    while (*p) {
        long v = strtol(p, &end, 10);
        if (end == p || v < 0 || v > 255 || (*end && *end != '.')) return -1;
        if (total < 16) address[total] = (unsigned char)v;
        total++;
        p = end;
        if (*p == '.') p++;
    }
    i = total == 16 || (allow_mask && total == 32);
    return i ? 0 : -1;
}

int modem_context_is_ims(const char *response, int cid)
{
    const char *p = response;
    char fields[16][256], *end;
    while ((p = strstr(p, "+CGCONTRDP:")) != NULL) {
        int n;
        long actual;
        p += 11;
        n = csv(p, fields, 16);
        if (n < 3) return -1;
        actual = strtol(fields[0], &end, 10);
        if (*end) return -1;
        if (actual == cid && (fields[2][0] == 'i' || fields[2][0] == 'I') &&
            (fields[2][1] == 'm' || fields[2][1] == 'M') &&
            (fields[2][2] == 's' || fields[2][2] == 'S') &&
            (fields[2][3] == '.' || fields[2][3] == 0)) return 1;
        while (*p && *p != '\n') p++;
    }
    return 0;
}

int modem_ims_pdn(const char *response, int cid, struct ims_pdn_info *info)
{
    const char *p = response;
    char fields[16][256], *end;
    long actual;
    int n, i;
    while ((p = strstr(p, "+CGCONTRDP:")) != NULL) {
        p += 11;
        n = csv(p, fields, 16);
        if (n < 9) return -1;
        actual = strtol(fields[0], &end, 10);
        if (*end) return -1;
        if (actual != cid) { while (*p && *p != '\n') p++; continue; }
        if (!((fields[2][0] == 'i' || fields[2][0] == 'I') &&
              (fields[2][1] == 'm' || fields[2][1] == 'M') &&
              (fields[2][2] == 's' || fields[2][2] == 'S') &&
              (fields[2][3] == '.' || fields[2][3] == 0))) return -1;
        memset(info, 0, sizeof(*info)); info->cid = cid;
        if (address6(fields[3], info->address, 1)) return -1;
        for (i = 7; i <= 8; i++) {
            if (!fields[i][0]) continue;
            if (address6(fields[i], info->pcscf[info->pcscf_count], 0)) return -1;
            info->pcscf_count++;
        }
        return info->pcscf_count ? 0 : -1;
    }
    return -1;
}
