#define _POSIX_C_SOURCE 200809L
#include "ims_pdn.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int ims_active, bad_state, incomplete_ims, activations, closed, polls, waits, no_response, begin_fails, web_succeeds, web_calls;
static time_t uptime = 100;

int ims_activate_begin(void) { activations++; return begin_fails ? -1 : 42; }
int ims_activate_poll(int queue)
{
    assert(queue == 42);
    polls++;
    if (no_response) return 0;
    if (polls == 23) ims_active = 1;
    return polls == 22 ? 2 : 0;
}
void ims_activate_end(int queue) { assert(queue == 42); closed++; }
unsigned int sleep(unsigned int seconds) { assert(seconds == 1); waits++; uptime += seconds; return 0; }
int clock_gettime(clockid_t clock, struct timespec *ts)
{
    assert(clock == CLOCK_MONOTONIC);
    ts->tv_sec = uptime; ts->tv_nsec = 0;
    return 0;
}

int at_http_command(struct at_http *at, const char *command, char *out, size_t size)
{
    (void)at;
    if (!strcmp(command, "AT+CGACT?")) {
        snprintf(out, size, bad_state ? "ERROR\r\n" :
                 ims_active ? "+CGACT: 1,1\r\n+CGACT: 3,1\r\nOK\r\n" :
                              "+CGACT: 1,1\r\n+CGACT: 3,0\r\nOK\r\n");
    } else if (!strcmp(command, "AT+CGCONTRDP=1")) {
        snprintf(out, size, "+CGCONTRDP: 1,5,\"internet\"\r\nOK\r\n");
    } else if (!strcmp(command, "AT+CGCONTRDP=3")) {
        snprintf(out, size, incomplete_ims ? "+CGCONTRDP: 3,5,\"ims\"\r\nOK\r\n" :
                            "+CGCONTRDP: 3,5,\"ims\",\"2001:db8::1\",\"\",\"\",\"\","
                            "\"2001:db8::2\",\"\"\r\nOK\r\n");
    } else assert(0);
    return 0;
}

int at_http_command_web(struct at_http *at, const char *command, char *out, size_t size)
{
    (void)at; (void)command; (void)out; (void)size;
    web_calls++;
    if (web_succeeds) { ims_active = 1; return 0; }
    return -1;
}

int main(void)
{
    struct at_http at = {0};
    struct ims_pdn_info info;
    FILE *interfaces;
    int i;
    ims_active = 1;
    assert(ims_pdn_acquire(&at, &info) == 0 && info.cid == 3);
    interfaces = tmpfile(); assert(interfaces);
    fputs("20010db8000000000000000000000009 06 40 00 80 wan3\n", interfaces);
    rewind(interfaces);
    assert(ims_pdn_local_interface(&info, interfaces) == 0);
    assert(!strcmp(info.interface, "wan3") && info.address[15] == 9);
    fclose(interfaces);
    incomplete_ims = 1;
    assert(ims_pdn_acquire(&at, &info) == -1 && activations == 0);
    incomplete_ims = 0;
    ims_active = 0;
    assert(ims_pdn_acquire(&at, &info) == -1);
    assert(activations == 1 && closed == 0 && polls == 20);
    assert(ims_pdn_acquire(&at, &info) == 0 && info.cid == 3);
    assert(activations == 1 && closed == 1 && polls == 23 && waits == 21);
    ims_active = 0; no_response = 1; polls = 0; uptime += 61;
    for (i = 0; i < 5; i++) assert(ims_pdn_acquire(&at, &info) == -1);
    assert(activations == 2 && closed == 2);
    no_response = 0; polls = 21;
    assert(ims_pdn_acquire(&at, &info) == 0 && info.cid == 3);
    assert(activations == 3 && closed == 3);
    ims_active = 0; no_response = 1; polls = 0; uptime += 61;
    assert(ims_pdn_acquire(&at, &info) == -1 && activations == 4 && closed == 3);
    ims_pdn_cleanup();
    assert(closed == 4);
    bad_state = 1;
    assert(ims_pdn_acquire(&at, &info) == -1);
    assert(activations == 4);
    bad_state = 0; ims_active = 0; begin_fails = 1; web_succeeds = 1; uptime += 61;
    assert(ims_pdn_acquire(&at, &info) == 0 && info.cid == 3);
    assert(activations == 5 && web_calls == 2);
    puts("ims_pdn: ok");
    return 0;
}
