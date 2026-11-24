#include "usim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void auth_result(const char *stage, const unsigned char *data, size_t length)
{
    char line[128];
    int n;
    if (length < 2) return;
    n = snprintf(line, sizeof(line), "%s: length %u, status %02X%02X.\n",
                 stage, (unsigned int)length, data[length - 2], data[length - 1]);
    if (n > 0 && (size_t)n < sizeof(line)) (void)write(STDERR_FILENO, line, (size_t)n);
}

struct usim_challenge {
    unsigned char value[35];
    struct usim_challenge *next;
};

static int terminal_ok(const char *response)
{
    const char *p = response;
    while ((p = strstr(p, "OK")) != NULL) {
        if ((p == response || p[-1] == '\n' || p[-1] == '\r') &&
            (!p[2] || p[2] == '\r' || p[2] == '\n')) return 1;
        p += 2;
    }
    return 0;
}

int usim_open(struct usim *sim, struct at_http *at)
{
    char result[1024], *p, *end;
    memset(sim, 0, sizeof(*sim));
    sim->at = at;
    if (at_http_command(at, "AT+CCHO=\"A0000000871002\"", result, sizeof(result)) ||
        !terminal_ok(result)) return -1;
    p = result;
    while (*p) {
        while (*p == '\r' || *p == '\n' || *p == ' ') p++;
        if (!strncmp(p, "+CCHO:", 6)) {
            p += 6;
            while (*p == ' ' || *p == '\t') p++;
        }
        if (*p >= '0' && *p <= '9') {
            long channel = strtol(p, &end, 10);
            while (*end == ' ') end++;
            if ((*end == '\r' || *end == '\n' || !*end) && channel >= 1 && channel <= 3) {
                sim->channel = (int)channel;
                return 0;
            }
        }
        while (*p && *p != '\n') p++;
        if (*p) p++;
    }
    return -1;
}

static int remember_challenge(struct usim *sim, const unsigned char *data)
{
    struct usim_challenge *entry;
    for (entry = sim->challenges; entry; entry = entry->next)
        if (!memcmp(entry->value, data, sizeof(entry->value))) return -1;
    entry = malloc(sizeof(*entry));
    if (!entry) return -1;
    memcpy(entry->value, data, sizeof(entry->value));
    entry->next = sim->challenges;
    sim->challenges = entry;
    return 0;
}

static int unhex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

int usim_transmit(struct usim *sim, const unsigned char *apdu, size_t len,
                  unsigned char *response, size_t *response_len)
{
    static const char digits[] = "0123456789ABCDEF";
    unsigned char data[256];
    char hex[513], command[560], result[2048], *p, *end;
    size_t i, count, capacity = *response_len;
    long chars, sequence;
    *response_len = 0;
    if (sim->channel < 1 || sim->channel > 3 || len < 4 || len > 255 || (apdu[0] & 0x43)) return -1;
    memcpy(data, apdu, len);
    data[0] |= (unsigned char)sim->channel;
    if (len == 39 && data[1] == 0x88 && data[2] == 0 && data[3] == 0x81 &&
        data[4] == 34 && data[5] == 16 && data[22] == 16) data[len++] = 0;
    if (data[1] == 0x88) {
        if (len != 40 || data[2] != 0 || data[3] != 0x81 || data[4] != 34 ||
            data[5] != 16 || data[22] != 16 || data[39] != 0 ||
            remember_challenge(sim, data + 4)) return -1;
    }
    for (i = 0; i < len; i++) {
        hex[i * 2] = digits[data[i] >> 4]; hex[i * 2 + 1] = digits[data[i] & 15];
    }
    hex[len * 2] = 0;
    snprintf(command, sizeof(command), "AT+ZSIM=%u,%s,0", (unsigned int)(len * 2), hex);
    if ((data[1] == 0x88 ? at_http_command_once : at_http_command)
        (sim->at, command, result, sizeof(result))) {
        if (data[1] == 0x88) (void)write(STDERR_FILENO, "SIM AUTH AT transport failed.\n", 30);
        return -1;
    }
    if (!terminal_ok(result)) {
        if (data[1] == 0x88) {
            const char *cme = strstr(result, "+CME ERROR:");
            char line[64];
            int n = cme ? snprintf(line, sizeof(line), "SIM AUTH CME error %ld.\n", strtol(cme + 11, NULL, 10)) :
                          snprintf(line, sizeof(line), "SIM AUTH AT result failed.\n");
            if (n > 0 && (size_t)n < sizeof(line)) (void)write(STDERR_FILENO, line, (size_t)n);
        }
        return -1;
    }
    p = strstr(result, "+ZSIM:");
    if (!p) return -1;
    p += 6; while (*p == ' ') p++;
    if (*p < '0' || *p > '9') return -1;
    chars = strtol(p, &end, 10);
    if (*end++ != ',' || chars < 4 || (chars & 1) || (size_t)chars / 2 > capacity) return -1;
    while (*end == ' ') end++;
    p = end;
    if (*p == '"') p++;
    if (strlen(p) < (size_t)chars) return -1;
    count = (size_t)chars / 2;
    for (i = 0; i < count; i++) {
        int hi = unhex(p[i * 2]), lo = unhex(p[i * 2 + 1]);
        if (hi < 0 || lo < 0) return -1;
        response[i] = (unsigned char)((hi << 4) | lo);
    }
    p += chars;
    if (*p == '"') p++;
    if (*p == ',') {
        sequence = strtol(p + 1, &end, 10);
        if (end == p + 1 || sequence != 0) return -1;
        p = end;
    }
    if (*p && *p != '\r' && *p != '\n') return -1;
    *response_len = count;
    return 0;
}

int usim_authenticate(struct usim *sim, const unsigned char rand[16],
                      const unsigned char autn[16], struct usim_aka_result *result)
{
    unsigned char apdu[40] = {0, 0x88, 0, 0x81, 34, 16};
    unsigned char answer[256], get_response[5] = {0, 0xc0, 0, 0, 0};
    size_t len = sizeof(answer), pos;
    int rc = -1;
    memset(result, 0, sizeof(*result));
    memcpy(apdu + 6, rand, 16); apdu[22] = 16; memcpy(apdu + 23, autn, 16);
    if (usim_transmit(sim, apdu, sizeof(apdu), answer, &len)) {
        (void)write(STDERR_FILENO, "SIM AUTH command failed.\n", 25); goto out;
    }
    auth_result("SIM AUTH response", answer, len);
    if (answer[len - 2] == 0x61 || answer[len - 2] == 0x9f) {
        get_response[4] = answer[len - 1];
        len = sizeof(answer);
        if (usim_transmit(sim, get_response, sizeof(get_response), answer, &len)) {
            (void)write(STDERR_FILENO, "SIM AUTH response retrieval failed.\n", 36); goto out;
        }
        auth_result("SIM AUTH retrieved", answer, len);
    }
    if (len < 4 || answer[len - 2] != 0x90 || answer[len - 1] != 0) goto out;
    len -= 2;
    if (answer[0] == 0xdc) {
        if (answer[1] == 0 && len == 2) result->status = USIM_AKA_REJECTED;
        else if (answer[1] == 14 && len == 16) {
            result->status = USIM_AKA_SYNC_FAILURE;
            memcpy(result->auts, answer + 2, 14);
        } else goto out;
        rc = 0; goto out;
    }
    if (answer[0] != 0xdb || answer[1] < 4 || answer[1] > 16) goto out;
    result->res_length = answer[1]; pos = 2;
    if (pos + result->res_length + 34 > len) goto out;
    memcpy(result->res, answer + pos, result->res_length); pos += result->res_length;
    if (answer[pos++] != 16) goto out;
    memcpy(result->ck, answer + pos, 16); pos += 16;
    if (answer[pos++] != 16) goto out;
    memcpy(result->ik, answer + pos, 16); pos += 16;
    if (len - pos == 9 && answer[pos] == 8) pos += 9;
    if (pos != len) goto out;
    result->status = USIM_AKA_SUCCESS; rc = 0;
out:
    memset(apdu, 0, sizeof(apdu));
    memset(answer, 0, sizeof(answer));
    if (rc) memset(result, 0, sizeof(*result));
    return rc;
}

int usim_close(struct usim *sim)
{
    struct usim_challenge *entry;
    char command[32], result[1024];
    int rc = 0;
    if (sim->channel) {
        snprintf(command, sizeof(command), "AT+CCHC=%d", sim->channel);
        if (at_http_command(sim->at, command, result, sizeof(result)) || !terminal_ok(result)) rc = -1;
    }
    while ((entry = sim->challenges) != NULL) {
        sim->challenges = entry->next;
        memset(entry, 0, sizeof(*entry));
        free(entry);
    }
    sim->channel = 0;
    return rc;
}
