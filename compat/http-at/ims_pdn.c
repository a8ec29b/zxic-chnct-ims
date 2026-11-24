#define _POSIX_C_SOURCE 200809L
#include "ims_pdn.h"
#include "ims_activate.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static time_t next_activation, activation_started;
static int activation_queue = -1, activation_event;

static time_t uptime_seconds(void)
{
    struct timespec ts;
    return clock_gettime(CLOCK_MONOTONIC, &ts) ? -1 : ts.tv_sec;
}

void ims_pdn_cleanup(void)
{
    if (activation_queue >= 0) ims_activate_end(activation_queue);
    activation_queue = -1;
    activation_event = 0;
}

static int discover(struct at_http *at, struct ims_pdn_info *info)
{
    char states[2048], response[4096], command[32];
    int cid, active, recognized = 0;
    if (at_http_command(at, "AT+CGACT?", states, sizeof(states))) return -1;
    if (!strstr(states, "+CGACT:") || !strstr(states, "OK")) return -1;
    for (cid = 1; cid <= 8; cid++) {
        active = modem_context_active(states, cid);
        if (active >= 0) recognized = 1;
        if (active != 1) continue;
        snprintf(command, sizeof(command), "AT+CGCONTRDP=%d", cid);
        if (at_http_command(at, command, response, sizeof(response))) return -1;
        if (!modem_ims_pdn(response, cid, info)) return 0;
        if (modem_context_is_ims(response, cid) != 0) return -1;
    }
    return recognized ? 1 : -1;
}

int ims_pdn_acquire(struct at_http *at, struct ims_pdn_info *info)
{
    int i, response, rc = discover(at, info);
    time_t now = uptime_seconds();
    if (now < 0) return -1;
    if (activation_queue < 0) {
        if (rc <= 0) return rc;
        if (now < next_activation) return -1;
        next_activation = now + 60;
        activation_queue = ims_activate_begin();
        if (activation_queue < 0) return -1;
        activation_started = now;
        activation_event = 0;
    }
    for (i = 0; i < 20; i++) {
        now = uptime_seconds();
        if (now < 0) return -1;
        if (now - activation_started >= 90) {
            ims_pdn_cleanup();
            return -1;
        }
        response = ims_activate_poll(activation_queue);
        if (response == 2) activation_event = 1;
        else if (response != 0) {
            ims_pdn_cleanup();
            if (response < 0) return -1;
            return discover(at, info) == 0 ? 0 : -1;
        }
        rc = discover(at, info);
        if (rc < 0) return -1;
        if (rc == 0 && activation_event) {
            ims_pdn_cleanup();
            return 0;
        }
        if (i != 19) sleep(1);
    }
    return -1;
}

int ims_pdn_local_interface(struct ims_pdn_info *info, FILE *interfaces)
{
    char line[256], hex[33], name[16], chosen[16];
    unsigned int index, prefix, scope, flags, value;
    unsigned char address[16], found[16];
    int matches = 0, i;
    snprintf(chosen, sizeof(chosen), "wan%d", info->cid);
    while (fgets(line, sizeof(line), interfaces)) {
        if (sscanf(line, "%32s %x %x %x %x %15s", hex, &index, &prefix, &scope, &flags, name) != 6 ||
            strlen(hex) != 32 || scope != 0 || strcmp(chosen, name)) continue;
        for (i = 0; i < 16; i++) {
            if (sscanf(hex + i * 2, "%2x", &value) != 1) break;
            address[i] = (unsigned char)value;
        }
        if (i != 16) continue;
        if (matches && memcmp(found, address, 16)) return -1;
        memcpy(found, address, 16); matches = 1;
    }
    if (!matches) return -1;
    memcpy(info->address, found, 16); strcpy(info->interface, chosen);
    return 0;
}
