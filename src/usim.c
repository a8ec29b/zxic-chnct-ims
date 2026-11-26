#define _POSIX_C_SOURCE 200809L
#include "usim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifndef USIM_STATE_PATH
#define USIM_STATE_PATH "/mnt/userdata/zxct-ims-start/aka.state"
#endif

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

struct persisted_state {
    unsigned char magic[4], challenge[35], candidate, pending, preferred;
};

static int save_state(struct usim *sim)
{
    struct persisted_state state = {{'A','K','A','1'}, {0}, 0, 0, 0};
    char path[sizeof(USIM_STATE_PATH) + 8];
    int fd, dir, rc = -1;
    memcpy(state.challenge, sim->last_challenge, sizeof(state.challenge));
    state.candidate = (unsigned char)sim->state_candidate;
    state.pending = (unsigned char)sim->state_pending;
    state.preferred = (unsigned char)sim->preferred;
    snprintf(path, sizeof(path), "%s.tmp", USIM_STATE_PATH);
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return -1;
    if (write(fd, &state, sizeof(state)) == sizeof(state) && !fsync(fd) && !close(fd)) {
        fd = -1;
        if (!rename(path, USIM_STATE_PATH)) {
            char *slash = strrchr(path, '/');
            if (slash) *slash = 0;
            dir = open(slash ? path : ".", O_RDONLY);
            if (dir >= 0) { rc = fsync(dir); close(dir); }
        }
    }
    if (fd >= 0) close(fd);
    if (rc) unlink(path);
    memset(&state, 0, sizeof(state));
    return rc;
}

static int load_state(struct usim *sim)
{
    struct persisted_state state;
    int fd = open(USIM_STATE_PATH, O_RDONLY);
    ssize_t n;
    if (fd < 0) return errno == ENOENT ? 0 : -1;
    n = read(fd, &state, sizeof(state)); close(fd);
    if (n != sizeof(state) || memcmp(state.magic, "AKA1", 4) ||
        state.candidate > 8 || state.preferred > 8 || state.pending > 1) return -1;
    memcpy(sim->last_challenge, state.challenge, sizeof(state.challenge));
    sim->state_loaded = 1; sim->state_candidate = state.candidate;
    sim->state_pending = state.pending; sim->preferred = state.preferred;
    memset(&state, 0, sizeof(state));
    return 0;
}

static int close_channel(struct usim *sim)
{
    char command[32], result[1024];
    if (!sim->channel) return 0;
    snprintf(command, sizeof(command), "AT+CCHC=%d", sim->channel);
    sim->channel = 0;
    if (at_http_command_on(sim->at, sim->transport, command, result, sizeof(result)) || !terminal_ok(result)) {
        sim->ccho_uncertain = 1;
        return -1;
    }
    return 0;
}

int usim_next(struct usim *sim)
{
    static const unsigned char select_mf[] = {0, 0xa4, 0, 0, 2, 0x3f, 0};
    unsigned char response[256];
    char result[1024], *p, *end;
    int tries, candidate;
    size_t length;
    (void)close_channel(sim);
    for (tries = 0; tries < 8; tries++) {
        candidate = (sim->candidate + 1) % 8;
        sim->candidate = candidate;
        if (sim->visited & (1 << candidate)) continue;
        sim->visited |= 1 << candidate;
        sim->mode = candidate / 2 + 1;
        sim->transport = candidate % 2 ? AT_TRANSPORT_WEB : AT_TRANSPORT_INTERNAL;
        if (at_http_command_on(sim->at, sim->transport, "AT", result, sizeof(result)) || !terminal_ok(result)) continue;
        if (sim->mode <= 2) {
            if (sim->ccho_uncertain) continue;
            if (at_http_command_on(sim->at, sim->transport, "AT+CCHO=\"A0000000871002\"", result, sizeof(result))) {
                sim->ccho_uncertain = 1;
                continue;
            }
            if (!terminal_ok(result)) continue;
            p = result;
            while (*p) {
                while (*p == '\r' || *p == '\n' || *p == ' ') p++;
                if (!strncmp(p, "+CCHO:", 6)) p += 6;
                while (*p == ' ' || *p == '\t') p++;
                if (*p >= '0' && *p <= '9') {
                    long channel = strtol(p, &end, 10);
                    while (*end == ' ') end++;
                    if ((*end == '\r' || *end == '\n' || !*end) && channel >= 1 && channel <= 3) {
                        sim->channel = (int)channel; break;
                    }
                }
                while (*p && *p != '\n') p++;
                if (*p) p++;
            }
            if (!sim->channel) { sim->ccho_uncertain = 1; continue; }
        }
        length = sizeof(response);
        if (!usim_transmit(sim, select_mf, sizeof(select_mf), response, &length)) {
            sim->aka_failed = 0;
            return 0;
        }
        (void)close_channel(sim);
    }
    sim->mode = 0;
    return -1;
}

int usim_open(struct usim *sim, struct at_http *at)
{
    memset(sim, 0, sizeof(*sim)); sim->at = at; sim->candidate = -1;
    if (load_state(sim)) return -1;
    if (sim->state_pending && sim->state_candidate) {
        sim->candidate = sim->state_candidate - 1;
        sim->visited |= 1 << sim->candidate;
    } else if (sim->preferred) sim->candidate = sim->preferred - 2;
    return usim_next(sim);
}

int usim_registered(struct usim *sim)
{
    sim->preferred = sim->candidate + 1;
    sim->visited = 1 << sim->candidate;
    return save_state(sim);
}

static int remember_challenge(struct usim *sim, const unsigned char *data)
{
    struct usim_challenge *entry;
    if (sim->state_loaded && !memcmp(sim->last_challenge, data, 35)) return -1;
    for (entry = sim->challenges; entry; entry = entry->next)
        if (!memcmp(entry->value, data, sizeof(entry->value))) return -1;
    memcpy(sim->last_challenge, data, 35);
    sim->state_candidate = sim->candidate + 1; sim->state_pending = 1;
    if (save_state(sim)) return -1;
    sim->state_loaded = 1;
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
    if (sim->mode < 1 || sim->mode > 4 || len < 4 || len > 255 || (apdu[0] & 0x43)) return -1;
    memcpy(data, apdu, len);
    if (sim->mode <= 2) data[0] |= (unsigned char)sim->channel;
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
    if (sim->mode == 1) snprintf(command, sizeof(command), "AT+ZSIM=%u,%s,0", (unsigned int)(len * 2), hex);
    else if (sim->mode == 2) snprintf(command, sizeof(command), "AT+ZSIM=%u,\"%s\",0", (unsigned int)(len * 2), hex);
    else if (sim->mode == 3) snprintf(command, sizeof(command), "AT+CSIM=%u,%s", (unsigned int)(len * 2), hex);
    else snprintf(command, sizeof(command), "AT+CSIM=%u,\"%s\"", (unsigned int)(len * 2), hex);
    if (data[1] == 0x88) sim->aka_failed = 1;
    if (at_http_command_on(sim->at, sim->transport, command, result, sizeof(result))) {
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
    p = strstr(result, sim->mode <= 2 ? "+ZSIM:" : "+CSIM:");
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
    if (len == 2 && answer[0] == 0x98 && answer[1] == 0x62) {
        result->status = USIM_AKA_REJECTED; rc = 0; goto out;
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
    if (!rc) {
        sim->aka_failed = 0; sim->state_pending = 0;
        if (save_state(sim)) rc = -1;
    }
    if (rc) memset(result, 0, sizeof(*result));
    return rc;
}

int usim_close(struct usim *sim)
{
    struct usim_challenge *entry;
    int rc = close_channel(sim);
    while ((entry = sim->challenges) != NULL) {
        sim->challenges = entry->next;
        memset(entry, 0, sizeof(*entry));
        free(entry);
    }
    sim->channel = 0; sim->mode = 0;
    return rc;
}
