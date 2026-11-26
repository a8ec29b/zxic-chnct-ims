#include "usim.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int calls, fail_command, aka_success, once_calls, supported_mode = 1, no_ccho;
static int mode_mask, transport_mask = 1, last_transport, ccho_calls, ccho_ambiguous;
static char last_command[600];

int at_http_command(struct at_http *at, const char *command, char *result, size_t size)
{
    (void)at;
    calls++;
    snprintf(last_command, sizeof(last_command), "%s", command);
    if (fail_command) return -1;
    if (strstr(command, "CCHO")) {
        ccho_calls++;
        if (ccho_ambiguous) return -1;
        if (no_ccho) { snprintf(result, size, "\r\nERROR\r\n"); return 0; }
        snprintf(result, size, "\r\n+CCHO: 1\r\n\r\nOK\r\n");
    }
    else if (strstr(command, "CCHC")) snprintf(result, size, "\r\nOK\r\n");
    else if (strstr(command, "A40000023F00")) {
        int mode = strstr(command, "AT+ZSIM=") ? (strchr(command, '"') ? 2 : 1) :
                   (strchr(command, '"') ? 4 : 3);
        if (mode_mask ? !(mode_mask & (1 << mode)) : mode != supported_mode) return -1;
        snprintf(result, size, "\r\n+%s: 4,\"6B00\"\r\n\r\nOK\r\n", mode <= 2 ? "ZSIM" : "CSIM");
    }
    else if (aka_success == 3 && strstr(command, "880081"))
        snprintf(result, size, "\r\n+%s: 4,\"9862\"\r\n\r\nOK\r\n", strstr(command, "CSIM") ? "CSIM" : "ZSIM");
    else if (aka_success == 4 && strstr(command, "880081"))
        snprintf(result, size, "\r\n+%s: 36,\"DC0E000102030405060708090A0B0C0D9000\"\r\n\r\nOK\r\n", strstr(command, "CSIM") ? "CSIM" : "ZSIM");
    else if (aka_success == 2 && strstr(command, "01880081"))
        snprintf(result, size, "\r\n+ZSIM: 110,\"DB08112233445566778810000102030405060708090A0B0C0D0E0F10101112131415161718191A1B1C1D1E1F0820212223242526279000\",0\r\n\r\nOK\r\n");
    else if (aka_success == 1 && strstr(command, "880081"))
        snprintf(result, size, "\r\n+%s: 84,\"DB041122334410000102030405060708090A0B0C0D0E0F10101112131415161718191A1B1C1D1E1F9000\",0\r\n\r\nOK\r\n", strstr(command, "CSIM") ? "CSIM" : "ZSIM");
    else snprintf(result, size, "\r\n+%s: 4,\"6A82\",0\r\n\r\nOK\r\n", strstr(command, "AT+CSIM=") ? "CSIM" : "ZSIM");
    return 0;
}

int at_http_command_once(struct at_http *at, const char *command, char *result, size_t size)
{
    once_calls++;
    return at_http_command(at, command, result, size);
}

int at_http_command_on(struct at_http *at, enum at_transport transport,
                       const char *command, char *result, size_t size)
{
    if (!(transport_mask & (1 << (transport - 1)))) return -1;
    last_transport = transport;
    if (!strcmp(command, "AT")) { snprintf(result, size, "\r\nOK\r\n"); return 0; }
    if (strstr(command, "880081")) once_calls++;
    return at_http_command(at, command, result, size);
}

int main(void)
{
    struct at_http at = {"127.0.0.1", NULL, {0}, 0, 0, 0, 0};
    struct usim sim;
    unsigned char select[] = {0x00, 0xa4, 0x00, 0x00, 0x02, 0x6f, 0x07};
    unsigned char challenge[39] = {0x00, 0x88, 0x00, 0x81, 34, 16};
    unsigned char response[100];
    unsigned char rand[16] = {0}, autn[16] = {0};
    struct usim_aka_result aka;
    size_t len;
    int before;
    unlink("/tmp/zxct-test-aka.state");
    challenge[22] = 16;
    assert(usim_open(&sim, &at) == 0);
    len = sizeof(response);
    assert(usim_transmit(&sim, select, sizeof(select), response, &len) == 0);
    assert(len == 2 && response[0] == 0x6a && response[1] == 0x82);
    assert(!strcmp(last_command, "AT+ZSIM=14,01A40000026F07,0"));

    /* An ambiguous transport failure must not replay the same AKA challenge. */
    fail_command = 1; len = sizeof(response);
    assert(usim_transmit(&sim, challenge, sizeof(challenge), response, &len) == -1);
    assert(once_calls == 1);
    assert(strstr(last_command, "AT+ZSIM=80,018800812210") == last_command);
    assert(strstr(last_command, "00,0") != NULL);
    before = calls; fail_command = 0; len = sizeof(response);
    assert(usim_transmit(&sim, challenge, sizeof(challenge), response, &len) == -1);
    assert(calls == before && once_calls == 1);
    challenge[10] = 1; len = sizeof(response);
    assert(usim_transmit(&sim, challenge, sizeof(challenge), response, &len) == 0);
    rand[0] = 2; aka_success = 1;
    assert(usim_authenticate(&sim, rand, autn, &aka) == 0);
    assert(aka.status == USIM_AKA_SUCCESS && aka.res_length == 4);
    assert(aka.res[0] == 0x11 && aka.ck[15] == 0x0f && aka.ik[0] == 0x10);
    rand[0] = 3; aka_success = 2;
    assert(usim_authenticate(&sim, rand, autn, &aka) == 0);
    assert(aka.status == USIM_AKA_SUCCESS && aka.res_length == 8 && aka.ik[0] == 0x10);
    assert(usim_close(&sim) == 0);
    for (supported_mode = 2; supported_mode <= 4; supported_mode++) {
        at.failed = 0; unlink("/tmp/zxct-test-aka.state");
        assert(usim_open(&sim, &at) == 0 && sim.mode == supported_mode && !at.failed);
        len = sizeof(response);
        assert(usim_transmit(&sim, select, sizeof(select), response, &len) == 0);
        if (supported_mode >= 3) assert(strstr(last_command, "00A40000026F07"));
        else assert(strstr(last_command, "01A40000026F07"));
        assert(usim_close(&sim) == 0);
    }
    unlink("/tmp/zxct-test-aka.state");
    no_ccho = 1; supported_mode = 3; at.failed = 0;
    assert(usim_open(&sim, &at) == 0 && sim.channel == 0 && sim.mode == 3 && !at.failed);
    challenge[10] = 9; fail_command = 1; len = sizeof(response); before = calls;
    assert(usim_transmit(&sim, challenge, sizeof(challenge), response, &len) == -1);
    assert(calls == before + 1 && strstr(last_command, "AT+CSIM=80,00880081") == last_command);
    fail_command = 0; len = sizeof(response); before = calls;
    assert(usim_transmit(&sim, challenge, sizeof(challenge), response, &len) == -1 && calls == before);
    assert(usim_close(&sim) == 0);
    unlink("/tmp/zxct-test-aka.state");
    no_ccho = 0; supported_mode = 0; at.failed = 0;
    assert(usim_open(&sim, &at) == -1);
    unlink("/tmp/zxct-test-aka.state");
    mode_mask = (1 << 1) | (1 << 3); transport_mask = 1;
    assert(usim_open(&sim, &at) == 0 && sim.mode == 1);
    rand[0] = 10; fail_command = 1;
    assert(usim_authenticate(&sim, rand, autn, &aka) == -1 && sim.aka_failed);
    fail_command = 0;
    assert(usim_next(&sim) == 0 && sim.mode == 3);
    before = once_calls;
    assert(usim_authenticate(&sim, rand, autn, &aka) == -1 && once_calls == before);
    rand[0] = 11; aka_success = 1;
    assert(usim_authenticate(&sim, rand, autn, &aka) == 0 && aka.status == USIM_AKA_SUCCESS);
    assert(usim_registered(&sim) == 0);
    assert(usim_close(&sim) == 0);
    assert(usim_open(&sim, &at) == 0 && sim.mode == 3);
    rand[0] = 12; fail_command = 1;
    assert(usim_authenticate(&sim, rand, autn, &aka) == -1 && sim.aka_failed);
    fail_command = 0; assert(usim_close(&sim) == 0);
    assert(usim_open(&sim, &at) == 0 && sim.mode == 1);
    before = once_calls;
    assert(usim_authenticate(&sim, rand, autn, &aka) == -1 && once_calls == before);
    rand[0] = 13; aka_success = 3;
    assert(usim_authenticate(&sim, rand, autn, &aka) == 0 && aka.status == USIM_AKA_REJECTED && !sim.aka_failed);
    rand[0] = 14; aka_success = 4;
    assert(usim_authenticate(&sim, rand, autn, &aka) == 0 && aka.status == USIM_AKA_SYNC_FAILURE && !sim.aka_failed);
    assert(usim_close(&sim) == 0);
    unlink("/tmp/zxct-test-aka.state");
    transport_mask = 2; aka_success = 1;
    assert(usim_open(&sim, &at) == 0 && sim.transport == AT_TRANSPORT_WEB);
    rand[0] = 15;
    assert(usim_authenticate(&sim, rand, autn, &aka) == 0 && last_transport == AT_TRANSPORT_WEB);
    assert(usim_close(&sim) == 0);
    unlink("/tmp/zxct-test-aka.state");
    transport_mask = 3; ccho_ambiguous = 1; ccho_calls = 0;
    assert(usim_open(&sim, &at) == 0 && sim.mode == 3 && ccho_calls == 1);
    assert(usim_close(&sim) == 0);
    unlink("/tmp/zxct-test-aka.state");
    puts("usim: ok");
    return 0;
}
