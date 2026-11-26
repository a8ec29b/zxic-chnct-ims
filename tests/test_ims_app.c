#include "ims_app.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *reply, *alternate_reply;
static int login_fails, calls, starts;
static const char *expected_imsi = "460110123456789";

int at_http_login(struct at_http *at, const char *password)
{
    (void)at; (void)password;
    calls++;
    return login_fails;
}

int at_http_command(struct at_http *at, const char *command, char *out, size_t size)
{
    (void)at;
    assert(!strcmp(command, "AT+CIMI"));
    calls++;
    if (!reply) return -1;
    snprintf(out, size, "%s", reply);
    return 0;
}

int at_http_command_alternate(struct at_http *at, const char *command, char *out, size_t size)
{
    (void)at; (void)command;
    if (!alternate_reply) return -1;
    snprintf(out, size, "%s", alternate_reply);
    return 0;
}

static int start(struct at_http *at, const char *imsi, void *context)
{
    (void)at; (void)context;
    assert(!strcmp(imsi, expected_imsi));
    starts++;
    return IMS_APP_OK;
}

int main(void)
{
    struct at_http at = {0};
    char password[8] = "test";
    reply = "460001234567890\r\nOK\r\n";
    assert(ims_app_run(&at, password, start, NULL) == IMS_APP_UNSUPPORTED_OPERATOR);
    assert(calls == 2 && starts == 0 && !password[0]);
    strcpy(password, "test");
    calls = 0; reply = NULL;
    assert(ims_app_run(&at, password, start, NULL) == IMS_APP_SIM_UNAVAILABLE);
    assert(calls == 2 && starts == 0);
    strcpy(password, "test");
    calls = 0; reply = "460110123456789\r\nOK\r\n";
    assert(ims_app_run(&at, password, start, NULL) == IMS_APP_OK);
    assert(calls == 2 && starts == 1);
    strcpy(password, "test");
    calls = 0; at.ipc_ready = 1; reply = "failure"; alternate_reply = "460110123456789\r\nOK\r\n";
    assert(ims_app_run(&at, password, start, NULL) == IMS_APP_OK);
    assert(calls == 2 && starts == 2 && at.ipc_ready);
    strcpy(password, "test");
    calls = 0; reply = "failure"; alternate_reply = "460011234567890\r\nOK\r\n";
    expected_imsi = "460011234567890";
    assert(ims_app_run(&at, password, start, NULL) == IMS_APP_OK);
    assert(calls == 2 && starts == 3);
    alternate_reply = NULL;
    strcpy(password, "test");
    calls = 0; reply = "460061234567890\r\nOK\r\n";
    expected_imsi = "460061234567890";
    assert(ims_app_run(&at, password, start, NULL) == IMS_APP_OK);
    assert(calls == 2 && starts == 4);
    strcpy(password, "test");
    calls = 0; login_fails = 1;
    assert(ims_app_run(&at, password, start, NULL) == IMS_APP_LOGIN_FAILED);
    assert(calls == 1 && starts == 4);
    puts("ims_app: ok");
    return 0;
}
