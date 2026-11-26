#include "ims_app.h"
#include "operator_gate.h"

#include <string.h>

int ims_app_run(struct at_http *at, char *password, ims_app_start_fn start, void *context)
{
    char imsi[16];
    enum operator_gate_result gate;
    int result;
    result = at_http_login(at, password);
    memset(password, 0, strlen(password));
    if (result) return IMS_APP_LOGIN_FAILED;
    gate = operator_gate(at, imsi);
    if (gate == OPERATOR_GATE_UNAVAILABLE) return IMS_APP_SIM_UNAVAILABLE;
    if (gate != OPERATOR_GATE_OK) return IMS_APP_UNSUPPORTED_OPERATOR;
    result = start(at, imsi, context);
    memset(imsi, 0, sizeof(imsi));
    return result;
}
